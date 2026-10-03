#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Attributes/KnightAttributeSet.h"
#include "Camera/CameraComponent.h"
#include "Camera/KnightCameraModeComponent.h"
#include "Character/KnightCharacter.h"
#include "CombatChallenge/KnightCombatFixture.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "Finisher/KnightFinisherComponent.h"
#include "Framework/KnightGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayAbilitySpec.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Input/KnightInputConfig.h"
#include "Misc/PackageName.h"
#include "MotionWarpingComponent.h"
#include "Party/KnightPartyState.h"
#include "PlayInEditorDataTypes.h"
#include "Player/KnightPlayerController.h"
#include "Player/KnightPlayerState.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tags/KnightGameplayTags.h"
#include "Traversal/KnightTraversalComponent.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace KnightCombatLifecycle
{
	const TCHAR* ChallengeMap = TEXT("/Game/Knight/Maps/Playables/L_M03_CombatChallenge");
	const TCHAR* DetachedMap = TEXT("/Game/Knight/Maps/Playables/L_M00_Boot");
	constexpr double PhaseTimeoutSeconds = 25.0;
	constexpr double StartupTimeoutSeconds = 60.0;
	constexpr double OwnerConvergenceTimeoutSeconds = 5.0;
	constexpr float OwnerLandingTolerance = 50.0f;
	constexpr float RequiredTraversalArc = 50.0f;
	constexpr uint16 ServerPort = 17903;
	const FVector TraversalSetup(-1210.0, 1200.0, 100.0);

	enum class EKnightCombatLifecyclePhase : uint8
	{
		Start, Ready, Position, Compete, Cancel, SecondFinisher, Disconnect, Rejoin,
		DeathFinisher, DeathCleanup, GuestRetry, Retry, FinisherStart, FinisherComplete,
		TraverseStart, TraverseAbort, TraverseCleanup,
		TraverseComplete, TraverseConverge, Cleanup, Complete
	};

	AKnightPartyState* FindParty(UWorld* World)
	{
		AKnightGameState* State = World != nullptr ? World->GetGameState<AKnightGameState>() : nullptr;
		return State != nullptr ? State->GetPartyState() : nullptr;
	}

	TArray<FGameplayAbilitySpecHandle> GetSpecs(const UKnightAbilitySystemComponent* ASC)
	{
		TArray<FGameplayAbilitySpecHandle> Result;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			Result.Add(Spec.Handle);
		}
		return Result;
	}

	bool HasInactiveAbilityInputs(const UKnightAbilitySystemComponent* ASC)
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.InputPressed || Spec.IsActive())
			{
				return false;
			}
		}
		return true;
	}

	/** Position changes below are explicit failure fixtures; traversal movement itself must come from root motion. */
	class FKnightCombatLifecycleCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FKnightCombatLifecycleCommand(FAutomationTestBase& InTest) : Test(InTest)
		{
		}

		virtual ~FKnightCombatLifecycleCommand() override
		{
			ReleaseExternalLocks();
			if (bOwnsSession && GEditor != nullptr)
			{
				if (GEditor->IsPlayingSessionInEditor())
				{
					GEditor->EndPlayMap();
				}
				else if (GEditor->IsPlaySessionRequestQueued())
				{
					GEditor->CancelRequestPlaySession();
				}
				RestoreWindows();
			}
		}

		virtual bool Update() override
		{
			if (Phase < EKnightCombatLifecyclePhase::Cleanup && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			if (Phase > EKnightCombatLifecyclePhase::Ready && Phase < EKnightCombatLifecyclePhase::Cleanup
				&& !HasCanonicalIdentities())
			{
				Fail(TEXT("Combat lifecycle lost canonical actors, IDs, hero definitions, ASC ownership or exact grants."));
			}
			if (Phase > EKnightCombatLifecyclePhase::Start && Phase < EKnightCombatLifecyclePhase::Cleanup
				&& Elapsed() > (Phase == EKnightCombatLifecyclePhase::Ready ? StartupTimeoutSeconds : PhaseTimeoutSeconds))
			{
				if (Phase == EKnightCombatLifecyclePhase::TraverseComplete
					|| Phase == EKnightCombatLifecyclePhase::TraverseConverge)
				{
					Fail(GetTraversalDiagnostics(TEXT("Actual physical traversal timed out")));
				}
				else if (Phase == EKnightCombatLifecyclePhase::GuestRetry || Phase == EKnightCombatLifecyclePhase::Retry)
				{
					Fail(GetRetryDiagnostics(TEXT("Actual host-owned retry timed out")));
				}
				else if (Phase == EKnightCombatLifecyclePhase::FinisherStart
					|| Phase == EKnightCombatLifecyclePhase::FinisherComplete)
				{
					Fail(GetFinisherDiagnostics(TEXT("Actual natural finisher completion timed out")));
				}
				else
				{
					Fail(FString::Printf(TEXT("Actual combat lifecycle timed out in phase %d."), static_cast<int32>(Phase)));
				}
			}
			switch (Phase)
			{
			case EKnightCombatLifecyclePhase::Start: Start(); break;
			case EKnightCombatLifecyclePhase::Ready: WaitReady(); break;
			case EKnightCombatLifecyclePhase::Position: WaitPosition(); break;
			case EKnightCombatLifecyclePhase::Compete: WaitCompete(); break;
			case EKnightCombatLifecyclePhase::Cancel: WaitCancel(); break;
			case EKnightCombatLifecyclePhase::SecondFinisher: WaitSecondFinisher(); break;
			case EKnightCombatLifecyclePhase::Disconnect: WaitDisconnect(); break;
			case EKnightCombatLifecyclePhase::Rejoin: WaitRejoin(); break;
			case EKnightCombatLifecyclePhase::DeathFinisher: WaitDeathFinisher(); break;
			case EKnightCombatLifecyclePhase::DeathCleanup: WaitDeathCleanup(); break;
			case EKnightCombatLifecyclePhase::GuestRetry: WaitGuestRetry(); break;
			case EKnightCombatLifecyclePhase::Retry: WaitRetry(); break;
			case EKnightCombatLifecyclePhase::FinisherStart: WaitFinisherStart(); break;
			case EKnightCombatLifecyclePhase::FinisherComplete: WaitFinisherComplete(); break;
			case EKnightCombatLifecyclePhase::TraverseStart: WaitTraverseStart(); break;
			case EKnightCombatLifecyclePhase::TraverseAbort: WaitTraverseAbort(); break;
			case EKnightCombatLifecyclePhase::TraverseCleanup: WaitTraverseCleanup(); break;
			case EKnightCombatLifecyclePhase::TraverseComplete: WaitTraverseComplete(); break;
			case EKnightCombatLifecyclePhase::TraverseConverge: WaitTraverseConverge(); break;
			case EKnightCombatLifecyclePhase::Cleanup: Cleanup(); break;
			default: break;
			}
			return Phase == EKnightCombatLifecyclePhase::Complete;
		}

	private:
		void Enter(EKnightCombatLifecyclePhase InPhase)
		{
			Phase = InPhase;
			PhaseStarted = FPlatformTime::Seconds();
		}
		double Elapsed() const
		{
			return FPlatformTime::Seconds() - PhaseStarted;
		}
		void Fail(const FString& Message)
		{
			Test.AddError(Message);
			BeginCleanup();
		}
		UWorld* ContextWorld(FName Handle) const
		{
			const FWorldContext* Context = GEngine != nullptr ? GEngine->GetWorldContextFromHandle(Handle) : nullptr;
			return Context != nullptr ? Context->World() : nullptr;
		}
		AKnightPlayerController* FindOwner(UWorld* World) const
		{
			if (World == nullptr || World->GetNetMode() != NM_Client)
			{
				return nullptr;
			}
			for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AKnightPlayerController* Player = Cast<AKnightPlayerController>(Iterator->Get());
				AKnightCharacter* Pawn = Player != nullptr ? Player->GetControlledKnight() : nullptr;
				AKnightPlayerState* Human = Player != nullptr ? Player->GetPlayerState<AKnightPlayerState>() : nullptr;
				AKnightPartyState* Party = FindParty(World);
				const FKnightRosterEntry* Entry = Human != nullptr && Party != nullptr
					? Party->FindSlot(Human->GetAssignedSlot()) : nullptr;
				if (Player != nullptr && Player->IsLocalController() && !Player->HasAuthority()
					&& Pawn != nullptr && Pawn->IsLocallyControlled() && Pawn->GetLocalRole() == ROLE_AutonomousProxy
					&& Entry != nullptr && Entry->Knight == Pawn && Entry->HumanPlayerState == Human
					&& Entry->KnightId == Pawn->GetKnightEntityId() && Pawn->GetCombatComponent()->GetProfile() != nullptr
					&& !GetSpecs(Pawn->GetKnightAbilitySystem()).IsEmpty())
				{
					return Player;
				}
			}
			return nullptr;
		}

		void Start()
		{
			if (GEditor == nullptr || GEngine == nullptr || GEditor->IsPlaySessionInProgress()
				|| !FPackageName::DoesPackageExist(ChallengeMap) || !FPackageName::DoesPackageExist(DetachedMap))
			{
				Fail(TEXT("Combat network gate requires an idle Editor and actual M3 bootstrap maps."));
				return;
			}
			const ULevelEditorPlaySettings* Defaults = GetDefault<ULevelEditorPlaySettings>();
			OriginalPositions = Defaults->MultipleInstancePositions;
			OriginalLastSize = Defaults->LastSize;
			OriginalWindowPosition = Defaults->NewWindowPosition;
			OriginalWidth = Defaults->NewWindowWidth;
			OriginalHeight = Defaults->NewWindowHeight;
			Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(Defaults, GetTransientPackage()));
			Settings->SetPlayNetMode(PIE_ListenServer);
			Settings->SetPlayNumberOfClients(3);
			Settings->SetRunUnderOneProcess(true);
			Settings->SetServerPort(ServerPort);
			Settings->bLaunchSeparateServer = false;
			Settings->EnableGameSound = false;
			Settings->GameGetsMouseControl = false;
			Settings->SetClientWindowSize(FIntPoint(640, 360));
			Settings->NetworkEmulationSettings.bIsNetworkEmulationEnabled = true;
			Settings->NetworkEmulationSettings.EmulationTarget = NetworkEmulationTarget::Any;
			Settings->NetworkEmulationSettings.CurrentProfile = TEXT("Custom");
			Settings->NetworkEmulationSettings.OutPackets.MinLatency = 50;
			Settings->NetworkEmulationSettings.OutPackets.MaxLatency = 100;
			Settings->NetworkEmulationSettings.OutPackets.PacketLossPercentage = 0;
			Settings->NetworkEmulationSettings.InPackets.MinLatency = 0;
			Settings->NetworkEmulationSettings.InPackets.MaxLatency = 0;
			Settings->NetworkEmulationSettings.InPackets.PacketLossPercentage = 0;
			FRequestPlaySessionParams Request;
			Request.EditorPlaySettings = Settings.Get();
			Request.GlobalMapOverride = ChallengeMap;
			Request.bAllowOnlineSubsystem = false;
			GEditor->RequestPlaySession(Request);
			bOwnsSession = true;
			Test.AddInfo(TEXT("Real listen-server/two-client combat gate requested with outgoing 50–100 ms packet latency."));
			Enter(EKnightCombatLifecyclePhase::Ready);
		}

		void WaitReady()
		{
			ClientContexts.Reset();
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
				{
					if (Context.World()->GetNetMode() == NM_ListenServer)
					{
						Server = Context.World();
					}
					if (Context.World()->GetNetMode() == NM_Client)
					{
						ClientContexts.Add(Context.ContextHandle);
					}
				}
			}
			AKnightPartyState* Party = FindParty(Server.Get());
			if (Party == nullptr || !Party->IsCanonicalRosterReady() || Party->GetHumanCount() != 3 || ClientContexts.Num() != 2)
			{
				return;
			}
			for (FName Context : ClientContexts)
			{
				AKnightPlayerController* Player = FindOwner(ContextWorld(Context));
				if (Player == nullptr)
				{
					return;
				}
				const ULocalPlayer* Local = Player->GetLocalPlayer();
				const UEnhancedInputLocalPlayerSubsystem* Input = Local != nullptr
					? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
				if (Input == nullptr || Player->GetKnightInputConfig() == nullptr
					|| !Input->HasMappingContext(Player->GetKnightInputConfig()->GetCombatMappingContext()))
				{
					return;
				}
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || Entry->Knight == nullptr || Entry->Knight->GetCombatComponent()->GetProfile() == nullptr)
				{
					return;
				}
			}
			for (TActorIterator<AKnightCombatFixture> Iterator(Server.Get()); Iterator; ++Iterator)
			{
				if (Iterator->GetFixtureKind() == EKnightCombatFixtureKind::MeleeTarget)
				{
					Target = *Iterator;
				}
			}
			if (!Target.IsValid() || !Target->GetInitializationResult().IsSuccess())
			{
				return;
			}
#if DO_ENABLE_NET_TEST
			if (Server->GetNetDriver()->PacketSimulationSettings.PktLagMin != 50
				|| Server->GetNetDriver()->PacketSimulationSettings.PktLagMax != 100)
			{
				Fail(TEXT("Requested packet latency was not actually applied to the real listen-server NetDriver."));
				return;
			}
#else
			Fail(TEXT("This Editor build cannot execute the required actual packet-latency gate."));
			return;
#endif
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				AKnightCharacter* Knight = Party->FindSlot(Slot)->Knight;
				Knights.Add(Knight);
				Ids.Add(Knight->GetKnightEntityId());
				Heroes.Add(Knight->GetHeroDefinitionId());
				Specs.Add(GetSpecs(Knight->GetKnightAbilitySystem()));
				Grants.Add(Knight->GetKnightAbilitySystem()->GetOwnedGrantCount());
			}
			for (int32 Index = 0; Index < 2; ++Index)
			{
				Clients.Add(FindOwner(ContextWorld(ClientContexts[Index])));
				Slots.Add(Clients.Last()->GetPlayerState<AKnightPlayerState>()->GetAssignedSlot());
				Place(Knights[Slots[Index]].Get(), FVector(-1210.0, -350.0 + Index * 110.0, 100.0), FRotator::ZeroRotator);
			}
			if (!PrepareTarget())
			{
				return;
			}
			Enter(EKnightCombatLifecyclePhase::Position);
		}

		bool HasCanonicalIdentities() const
		{
			AKnightPartyState* Party = FindParty(Server.Get());
			if (Party == nullptr || Knights.Num() != 3 || !Target.IsValid())
			{
				return false;
			}
			int32 ActorCount = 0;
			for (TActorIterator<AKnightCharacter> Iterator(Server.Get()); Iterator; ++Iterator)
			{
				++ActorCount;
			}
			if (ActorCount != 3)
			{
				return false;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || Entry->Knight != Knights[Slot].Get() || Entry->KnightId != Ids[Slot]
					|| Entry->HeroDefinitionId != Heroes[Slot] || !Knights[Slot].IsValid()
					|| Knights[Slot]->GetKnightAbilitySystem()->GetOwnerActor() != Knights[Slot].Get()
					|| Knights[Slot]->GetKnightAbilitySystem()->GetAvatarActor() != Knights[Slot].Get()
					|| GetSpecs(Knights[Slot]->GetKnightAbilitySystem()) != Specs[Slot]
					|| Knights[Slot]->GetKnightAbilitySystem()->GetOwnedGrantCount() != Grants[Slot])
				{
					return false;
				}
			}
			return true;
		}
		void Place(AKnightCharacter* Pawn, const FVector& Location, const FRotator& Rotation)
		{
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Pawn->GetCharacterMovement()->StopMovementImmediately();
			Pawn->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
			Pawn->ForceNetUpdate();
		}
		bool PrepareTarget()
		{
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			FKnightResult Result = Target->ResetForCheckpoint();
			if (Result.IsSuccess())
			{
				Result = UKnightAbilitySystemComponent::ApplyCombatDamage(Knights[Slots[0]].Get(), Target.Get(),
					Target->GetKnightAttributeSet()->GetDefense() + 1.0f,
					Target->GetCombatComponent()->GetProfile()->GetTuning().MaxPoise + 1.0f, false);
			}
			if (!Result.IsSuccess() || !Target->GetCombatComponent()->GetSnapshot().bStaggered)
			{
				Fail(TEXT("Validated shared GAS damage did not produce the real staggered finisher fixture."));
				return false;
			}
			TargetHealth = Target->GetKnightAttributeSet()->GetHealth();
			return true;
		}
		void SendInput(int32 ClientIndex, FGameplayTag Tag)
		{
			FScopedConditionalWorldSwitcher Switch(Clients[ClientIndex]->GetWorld());
			Clients[ClientIndex]->RequestAbilityInput(Tag, true);
			Clients[ClientIndex]->RequestAbilityInput(Tag, false);
		}
		void WaitPosition()
		{
			for (int32 Index = 0; Index < 2; ++Index)
			{
				if (!Clients[Index].IsValid() || FVector::Dist(Clients[Index]->GetControlledKnight()->GetActorLocation(),
					Knights[Slots[Index]]->GetActorLocation()) > 20.0f)
					{
						return;
					}
			}
			SendInput(0, KnightGameplayTags::Input_Combat_Finisher);
			SendInput(1, KnightGameplayTags::Input_Combat_Finisher);
			Enter(EKnightCombatLifecyclePhase::Compete);
		}
		void WaitCompete()
		{
			int32 ActiveCount = 0;
			for (int32 Index = 0; Index < 2; ++Index)
			{
				if (Knights[Slots[Index]]->FindComponentByClass<UKnightFinisherComponent>()->HasActiveFinisher())
				{
					++ActiveCount;
					Winner = Index;
				}
			}
			if (ActiveCount > 1)
			{
				Fail(TEXT("Simultaneous actual client RPCs acquired two reservations on one victim."));
				return;
			}
			if (ActiveCount != 1)
			{
				return;
			}
			AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			AKnightCharacter* Replica = Clients[Winner]->GetControlledKnight();
			const FGuid Reservation = Pawn->FindComponentByClass<UKnightFinisherComponent>()->GetReservation();
			UAnimMontage* SourceMontage = Pawn->GetKnightAbilitySystem()->GetCurrentMontage();
			UAnimMontage* TargetMontage = Target->GetKnightAbilitySystem()->GetCurrentMontage();
			AKnightCombatFixture* VictimReplica = nullptr;
			for (TActorIterator<AKnightCombatFixture> Iterator(Replica->GetWorld()); Iterator; ++Iterator)
			{
				if (Iterator->GetFixtureId() == Target->GetFixtureId())
				{
					VictimReplica = *Iterator;
				}
			}
			if (Target->FindComponentByClass<UKnightFinisherComponent>()->GetReservation() != Reservation
				|| Replica->FindComponentByClass<UKnightFinisherComponent>()->GetReservation() != Reservation
				|| Replica->GetCameraModeComponent()->GetModeCount() != 1 || !Clients[Winner]->IsMoveInputIgnored()
				|| !Clients[Winner]->IsLookInputIgnored() || SourceMontage == nullptr || TargetMontage == nullptr
				|| Replica->GetKnightAbilitySystem()->GetCurrentMontage() != SourceMontage
				|| !Replica->GetMesh()->GetAnimInstance()->Montage_IsPlaying(SourceMontage)
				|| VictimReplica == nullptr || !VictimReplica->GetMesh()->GetAnimInstance()->Montage_IsPlaying(TargetMontage)
				|| !Pawn->GetMesh()->GetAnimInstance()->Montage_IsPlaying(SourceMontage)
				|| !Target->GetMesh()->GetAnimInstance()->Montage_IsPlaying(TargetMontage))
			{
				return;
			}
			Test.AddInfo(TEXT("Two actual owning RPCs under packet latency yielded one GUID, two real montages and owner camera locks."));
			ExternalLockOwner = Clients[Winner];
			ExternalLockOwner->SetIgnoreMoveInput(true);
			ExternalLockOwner->SetIgnoreLookInput(true);
			bExternalLocks = true;
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Pawn->GetKnightAbilitySystem()->CancelAllAbilities();
			Enter(EKnightCombatLifecyclePhase::Cancel);
		}
		bool HasCleanFinisher() const
		{
			if (Target->FindComponentByClass<UKnightFinisherComponent>()->IsReserved()
				|| Target->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Finishing)
				|| !FMath::IsNearlyEqual(TargetHealth, Target->GetKnightAttributeSet()->GetHealth(), 0.01f))
			{
				return false;
			}
			for (const TWeakObjectPtr<AKnightCharacter>& Pawn : Knights)
			{
				if (Pawn->FindComponentByClass<UKnightFinisherComponent>()->IsReserved()
					|| Pawn->GetCameraModeComponent()->GetModeCount() != 0
					|| Pawn->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Finishing))
				{
					return false;
				}
				if (!HasInactiveAbilityInputs(Pawn->GetKnightAbilitySystem()))
				{
					return false;
				}
			}
			return true;
		}
		void WaitCancel()
		{
			if (!HasCleanFinisher() || Clients[Winner]->GetControlledKnight()->GetCameraModeComponent()->GetModeCount() != 0)
			{
				return;
			}
			if (!Clients[Winner]->IsMoveInputIgnored() || !Clients[Winner]->IsLookInputIgnored())
			{
				Fail(TEXT("Finisher cleanup removed another input owner's outstanding lock.")); return;
			}
			ReleaseExternalLocks();
			if (Clients[Winner]->IsMoveInputIgnored() || Clients[Winner]->IsLookInputIgnored())
			{
				Fail(TEXT("Finisher leaked its own input lock after exact balancing.")); return;
			}
			Test.AddInfo(TEXT("Cancel left target health unchanged and released both tags/reservation/camera; "
				"external input counters survived."));
			if (!PrepareTarget())
			{
				return;
			}
			SendInput(Winner, KnightGameplayTags::Input_Combat_Finisher);
			Enter(EKnightCombatLifecyclePhase::SecondFinisher);
		}
		void WaitSecondFinisher()
		{
			AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			if (!Pawn->FindComponentByClass<UKnightFinisherComponent>()->HasActiveFinisher()
				|| Clients[Winner]->GetControlledKnight()->GetCameraModeComponent()->GetModeCount() != 1)
			{
				return;
			}
			const TSharedPtr<const FInternetAddr> Address = Server->GetNetDriver()->GetLocalAddr();
			if (!Address.IsValid())
			{
				Fail(TEXT("Real listen server has no bound network address for JIP."));
				return;
			}
			ReconnectURL = FString::Printf(TEXT("127.0.0.1:%d?KnightSlot=%d"), Address->GetPort(), Slots[Winner]);
			FScopedConditionalWorldSwitcher Switch(Clients[Winner]->GetWorld());
			Clients[Winner]->ClientTravel(DetachedMap, TRAVEL_Absolute);
			Enter(EKnightCombatLifecyclePhase::Disconnect);
		}
		void WaitDisconnect()
		{
			UWorld* Detached = ContextWorld(ClientContexts[Winner]);
			const FKnightRosterEntry* Entry = FindParty(Server.Get())->FindSlot(Slots[Winner]);
			if (Detached == nullptr || Detached->GetNetMode() != NM_Standalone || Detached->GetNetDriver() != nullptr
				|| Entry->OwnerKind != EKnightControllerOwner::Companion || !HasCleanFinisher())
			{
				return;
			}
			if (Detached->GetFirstPlayerController() == nullptr)
			{
				return;
			}
			Test.AddInfo(TEXT("Real mid-finisher disconnect canceled both montages/reservation without damage "
				"and preserved canonical AI takeover."));
			FScopedConditionalWorldSwitcher Switch(Detached);
			Detached->GetFirstPlayerController()->ClientTravel(ReconnectURL, TRAVEL_Absolute);
			Enter(EKnightCombatLifecyclePhase::Rejoin);
		}
		void WaitRejoin()
		{
			AKnightPlayerController* Owner = FindOwner(ContextWorld(ClientContexts[Winner]));
			if (Owner == nullptr || Owner->GetPlayerState<AKnightPlayerState>()->GetAssignedSlot() != Slots[Winner]
				|| GetSpecs(Owner->GetControlledKnight()->GetKnightAbilitySystem()) != Specs[Slots[Winner]]
				|| Owner->GetControlledKnight()->GetCameraModeComponent()->GetModeCount() != 0
				|| Owner->IsMoveInputIgnored() || Owner->IsLookInputIgnored())
			{
				return;
			}
			Clients[Winner] = Owner;
			Test.AddInfo(TEXT("Same-slot real JIP received original combat/foundation specs and zero stale finisher camera/input state."));
			if (!PrepareTarget())
			{
				return;
			}
			Place(Knights[Slots[Winner]].Get(), FVector(-1210.0, -300.0, 100.0), FRotator::ZeroRotator);
			SendInput(Winner, KnightGameplayTags::Input_Combat_Finisher);
			Enter(EKnightCombatLifecyclePhase::DeathFinisher);
		}
		void WaitDeathFinisher()
		{
			AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			if (!Pawn->FindComponentByClass<UKnightFinisherComponent>()->HasActiveFinisher())
			{
				return;
			}
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			FKnightResult Result = Pawn->GetLifeComponent()->TrySetLifeTuning(0.2f, 2.0f, 300.0f, 0.35f);
			if (Result.IsSuccess())
			{
				Result = UKnightAbilitySystemComponent::ApplyDamage(Target.Get(), Pawn,
					Pawn->GetKnightAttributeSet()->GetMaxHealth() + Pawn->GetKnightAttributeSet()->GetDefense() + 1.0f);
			}
			if (!Result.IsSuccess())
			{
				Fail(TEXT("GAS death interrupt fixture was rejected."));
				return;
			}
			Enter(EKnightCombatLifecyclePhase::DeathCleanup);
		}
		void WaitDeathCleanup()
		{
			AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			if (!Pawn->GetLifeComponent()->IsDead() || !HasCleanFinisher()
				|| Clients[Winner]->GetControlledKnight()->GetCameraModeComponent()->GetModeCount() != 0)
			{
				return;
			}
			if (Pawn->GetCharacterMovement()->MovementMode != MOVE_None)
			{
				Fail(TEXT("Finisher interruption restored walking on a dead Knight.")); return;
			}
			Test.AddInfo(TEXT("Real GAS down/bleedout during finisher releases paired reservation/camera and keeps dead movement disabled."));
			FScopedConditionalWorldSwitcher Switch(Clients[Winner]->GetWorld());
			Clients[Winner]->RequestRestartDrill();
			Enter(EKnightCombatLifecyclePhase::GuestRetry);
		}
		void WaitGuestRetry()
		{
			const FKnightResult& GuestResult = Clients[Winner]->GetLastCommandResult();
			if (GuestResult.Context != TEXT("ChallengeRetry"))
			{
				return;
			}
			AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			if (GuestResult.Error != EKnightError::NotOwned || !Pawn->GetLifeComponent()->IsDead()
				|| Pawn->GetCharacterMovement()->MovementMode != MOVE_None
				|| Pawn->GetKnightAttributeSet()->GetHealth() > 0.0f || !HasCleanFinisher())
			{
				Fail(GetRetryDiagnostics(TEXT("Guest retry violated host ownership or mutated dead state")));
				return;
			}
			AKnightPlayerController* Host = nullptr;
			for (FConstPlayerControllerIterator Iterator = Server->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AKnightPlayerController* Candidate = Cast<AKnightPlayerController>(Iterator->Get());
				if (Candidate != nullptr && Candidate->HasAuthority() && Candidate->IsLocalController()
					&& Candidate->GetControlledKnight() != nullptr)
				{
					Host = Candidate;
					break;
				}
			}
			if (Host == nullptr)
			{
				Fail(TEXT("Actual listen server has no canonical host controller authorized to retry."));
				return;
			}
			Test.AddInfo(TEXT("Actual guest retry RPC returned NotOwned/ChallengeRetry and preserved dead health/movement."));
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Host->RequestRestartDrill();
			HostRetryResult = Host->GetLastCommandResult();
			if (!HostRetryResult.IsSuccess())
			{
				Fail(GetRetryDiagnostics(TEXT("Actual authorized host retry was rejected")));
				return;
			}
			Enter(EKnightCombatLifecyclePhase::Retry);
		}
		void WaitRetry()
		{
			AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			if (!Pawn->GetLifeComponent()->IsAlive() || !Pawn->GetCharacterMovement()->IsMovingOnGround()
				|| !Clients[Winner]->GetControlledKnight()->GetLifeComponent()->IsAlive())
			{
				return;
			}
			Test.AddInfo(GetRetryDiagnostics(TEXT("Authorized host retry restored original server/client Knight life and movement")));
			Place(Pawn, FVector(-1210.0, -300.0, 100.0), FRotator::ZeroRotator);
			Enter(EKnightCombatLifecyclePhase::FinisherStart);
		}
		AKnightCombatFixture* FindVictimReplica(UWorld* World) const
		{
			if (World != nullptr)
			{
				for (TActorIterator<AKnightCombatFixture> Iterator(World); Iterator; ++Iterator)
				{
					if (Iterator->GetFixtureId() == Target->GetFixtureId())
					{
						return *Iterator;
					}
				}
			}
			return nullptr;
		}
		void WaitFinisherStart()
		{
			AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			AKnightCharacter* Replica = Clients[Winner]->GetControlledKnight();
			if (Replica == nullptr || !Pawn->GetCharacterMovement()->IsMovingOnGround()
				|| !Replica->GetCharacterMovement()->IsMovingOnGround()
				|| FVector::Dist(Pawn->GetActorLocation(), Replica->GetActorLocation()) > 20.0f)
			{
				return;
			}
			if (!PrepareTarget())
			{
				return;
			}
			SendInput(Winner, KnightGameplayTags::Input_Combat_Finisher);
			Enter(EKnightCombatLifecyclePhase::FinisherComplete);
		}
		void WaitFinisherComplete()
		{
			AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			AKnightCharacter* Replica = Clients[Winner]->GetControlledKnight();
			AKnightCombatFixture* VictimReplica = FindVictimReplica(Clients[Winner]->GetWorld());
			UKnightFinisherComponent* Finisher = Pawn->FindComponentByClass<UKnightFinisherComponent>();
			if (Replica == nullptr || VictimReplica == nullptr)
			{
				return;
			}
			if (Finisher->HasActiveFinisher())
			{
				CompletedFinisherHandle = Finisher->GetReservation();
				UAnimMontage* Montage = Pawn->GetKnightAbilitySystem()->GetCurrentMontage();
				bNaturalFinisherObserved = bNaturalFinisherObserved || (Montage != nullptr
					&& Replica->GetKnightAbilitySystem()->GetCurrentMontage() == Montage
					&& Replica->GetMesh()->GetAnimInstance()->Montage_IsPlaying(Montage)
					&& Replica->FindComponentByClass<UKnightFinisherComponent>()->GetReservation() == CompletedFinisherHandle
					&& VictimReplica->FindComponentByClass<UKnightFinisherComponent>()->GetReservation() == CompletedFinisherHandle);
				return;
			}
			if (Pawn->GetCombatComponent()->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::Finisher) != 1
				|| !Target->GetLifeComponent()->IsDead() || Target->GetKnightAttributeSet()->GetHealth() != 0.0f
				|| Finisher->IsReserved() || Target->FindComponentByClass<UKnightFinisherComponent>()->IsReserved()
				|| Pawn->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Finishing)
				|| Target->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Finishing)
				|| !HasInactiveAbilityInputs(Pawn->GetKnightAbilitySystem())
				|| !Pawn->GetLifeComponent()->IsAlive() || !Pawn->GetCharacterMovement()->IsMovingOnGround()
				|| Pawn->GetCameraModeComponent()->GetModeCount() != 0
				|| Replica->FindComponentByClass<UKnightFinisherComponent>()->IsReserved()
				|| VictimReplica->FindComponentByClass<UKnightFinisherComponent>()->IsReserved()
				|| !VictimReplica->GetLifeComponent()->IsDead() || VictimReplica->GetKnightAttributeSet()->GetHealth() != 0.0f
				|| Replica->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Finishing)
				|| VictimReplica->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Finishing)
				|| Replica->GetCameraModeComponent()->GetModeCount() != 0
				|| Clients[Winner]->IsMoveInputIgnored() || Clients[Winner]->IsLookInputIgnored())
			{
				return;
			}
			if (!bNaturalFinisherObserved || !CompletedFinisherHandle.IsValid())
			{
				Fail(GetFinisherDiagnostics(TEXT("Natural finisher completed without observing the real owning montage and paired GUID")));
				return;
			}
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			const FKnightResult StaleResult = Finisher->EndFinisher(CompletedFinisherHandle, true);
			if (StaleResult.Error != EKnightError::NotOwned || Target->GetKnightAttributeSet()->GetHealth() != 0.0f
				|| !Target->GetLifeComponent()->IsDead() || Finisher->IsReserved()
				|| Target->FindComponentByClass<UKnightFinisherComponent>()->IsReserved()
				|| !HasInactiveAbilityInputs(Pawn->GetKnightAbilitySystem())
				|| Pawn->GetCombatComponent()->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::Finisher) != 1
				|| Pawn->GetCameraModeComponent()->GetModeCount() != 0 || !Pawn->GetCharacterMovement()->IsMovingOnGround())
			{
				Fail(GetFinisherDiagnostics(TEXT("Completed finisher handle mutated life, ability or ownership state again")));
				return;
			}
			Test.AddInfo(GetFinisherDiagnostics(TEXT("Natural finisher ended its GAS spec and released both replicated reservations")));
			Test.AddInfo(TEXT("Stale completed finisher GUID returned NotOwned without a second outcome or life/camera/input mutation."));
			Place(Pawn, TraversalSetup, FRotator::ZeroRotator);
			Enter(EKnightCombatLifecyclePhase::TraverseStart);
		}
		FString GetFinisherDiagnostics(const TCHAR* Context) const
		{
			const AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			const AKnightCharacter* Replica = Clients[Winner]->GetControlledKnight();
			const AKnightCombatFixture* VictimReplica = FindVictimReplica(Clients[Winner]->GetWorld());
			return FString::Printf(TEXT("%s: phase=%d accepted=%s source=%s victim=%s owner=%s victimReplica=%s "
				"sourceInputsInactive=%d outcome=%d victimLife=%d victimHealth=%.1f ownerCamera=%d observedMontage=%d."),
				Context, static_cast<int32>(Phase), *CompletedFinisherHandle.ToString(),
				*Pawn->FindComponentByClass<UKnightFinisherComponent>()->GetReservation().ToString(),
				*Target->FindComponentByClass<UKnightFinisherComponent>()->GetReservation().ToString(),
				Replica != nullptr ? *Replica->FindComponentByClass<UKnightFinisherComponent>()->GetReservation().ToString()
					: TEXT("<missing>"),
				VictimReplica != nullptr
					? *VictimReplica->FindComponentByClass<UKnightFinisherComponent>()->GetReservation().ToString() : TEXT("<missing>"),
				HasInactiveAbilityInputs(Pawn->GetKnightAbilitySystem()),
				Pawn->GetCombatComponent()->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::Finisher),
				static_cast<int32>(Target->GetLifeComponent()->GetLifeState()), Target->GetKnightAttributeSet()->GetHealth(),
				Replica != nullptr ? Replica->GetCameraModeComponent()->GetModeCount() : INDEX_NONE, bNaturalFinisherObserved);
		}
		FString GetRetryDiagnostics(const TCHAR* Context) const
		{
			const AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			const AKnightCharacter* Replica = Clients[Winner]->GetControlledKnight();
			const FKnightResult& GuestResult = Clients[Winner]->GetLastCommandResult();
			return FString::Printf(TEXT("%s: phase=%d guestError=%d guestContext=%s guestMessage=%s "
				"hostError=%d hostContext=%s hostMessage=%s serverLife=%d ownerLife=%d serverHealth=%.1f ownerHealth=%.1f "
				"serverMode=%d ownerMode=%d server=%s owner=%s."),
				Context, static_cast<int32>(Phase), static_cast<int32>(GuestResult.Error),
				*GuestResult.Context.ToString(), *GuestResult.Message.ToString(), static_cast<int32>(HostRetryResult.Error),
				*HostRetryResult.Context.ToString(), *HostRetryResult.Message.ToString(),
				static_cast<int32>(Pawn->GetLifeComponent()->GetLifeState()),
				Replica != nullptr ? static_cast<int32>(Replica->GetLifeComponent()->GetLifeState()) : INDEX_NONE,
				Pawn->GetKnightAttributeSet()->GetHealth(), Replica != nullptr ? Replica->GetKnightAttributeSet()->GetHealth() : -1.0f,
				static_cast<int32>(Pawn->GetCharacterMovement()->MovementMode),
				Replica != nullptr ? static_cast<int32>(Replica->GetCharacterMovement()->MovementMode) : INDEX_NONE,
				*Pawn->GetActorLocation().ToString(),
				Replica != nullptr ? *Replica->GetActorLocation().ToString() : TEXT("<missing>"));
		}
		void WaitTraverseStart()
		{
			AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			if (!Pawn->GetCharacterMovement()->IsMovingOnGround()
				|| FVector::Dist(Pawn->GetActorLocation(), Clients[Winner]->GetControlledKnight()->GetActorLocation()) > 20.0f)
			{
				return;
			}
			TraversalStart = Pawn->GetActorLocation();
			SendInput(Winner, KnightGameplayTags::Input_World_Traverse);
			Enter(EKnightCombatLifecyclePhase::TraverseAbort);
		}
		void WaitTraverseAbort()
		{
			AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			UKnightTraversalComponent* Traversal = Pawn->FindComponentByClass<UKnightTraversalComponent>();
			AKnightCharacter* Replica = Clients[Winner]->GetControlledKnight();
			UAnimMontage* Montage = Pawn->GetKnightAbilitySystem()->GetCurrentMontage();
			if (!Traversal->HasActiveTraversal() || Pawn->GetActorLocation().Z < TraversalStart.Z + 15.0f
				|| Montage == nullptr || Replica->GetKnightAbilitySystem()->GetCurrentMontage() != Montage
				|| !Replica->GetMesh()->GetAnimInstance()->Montage_IsPlaying(Montage))
			{
				return;
			}
			if (Pawn->GetCharacterMovement()->MovementMode != MOVE_Flying
				|| Pawn->FindComponentByClass<UMotionWarpingComponent>()->GetWarpTargets().Num() != 1)
			{
				Fail(TEXT("Active traversal has no real Flying root-motion/MotionWarping contract.")); return;
			}
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Pawn->GetKnightAbilitySystem()->CancelAllAbilities();
			Enter(EKnightCombatLifecyclePhase::TraverseCleanup);
		}
		void WaitTraverseCleanup()
		{
			AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			AKnightCharacter* Replica = Clients[Winner]->GetControlledKnight();
			if (Pawn->FindComponentByClass<UKnightTraversalComponent>()->HasActiveTraversal()
				|| Pawn->FindComponentByClass<UMotionWarpingComponent>()->GetWarpTargets().Num() != 0
				|| Pawn->GetCameraModeComponent()->GetModeCount() != 0 || Replica->GetCameraModeComponent()->GetModeCount() != 0
				|| Replica->FindComponentByClass<UMotionWarpingComponent>()->GetWarpTargets().Num() != 0
				|| Pawn->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Traversing))
			{
				return;
			}
			Test.AddInfo(TEXT("Mid-arc native GAS cancel removed server/client warp targets, traversal tag and owner camera/input mode."));
			Place(Pawn, TraversalSetup, FRotator::ZeroRotator);
			TraversalStart = TraversalSetup;
			MaximumTraversalZ = TraversalSetup.Z;
			bTraversalStarted = false;
			bTraversalSent = false;
			Enter(EKnightCombatLifecyclePhase::TraverseComplete);
		}
		void WaitTraverseComplete()
		{
			AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			AKnightCharacter* Replica = Clients[Winner]->GetControlledKnight();
			UKnightTraversalComponent* Traversal = Pawn->FindComponentByClass<UKnightTraversalComponent>();
			if (Replica == nullptr)
			{
				Fail(GetTraversalDiagnostics(TEXT("Owning traversal pawn was lost")));
				return;
			}
			if (!bTraversalSent)
			{
				if (!Pawn->GetCharacterMovement()->IsMovingOnGround()
					|| FVector::Dist(Pawn->GetActorLocation(), Clients[Winner]->GetControlledKnight()->GetActorLocation()) > 20.0f)
				{
					return;
				}
				TraversalStart = Pawn->GetActorLocation();
				OwnerTraversalStart = Replica->GetActorLocation();
				MaximumOwnerTraversalZ = OwnerTraversalStart.Z;
				bOwnerTraversalMontage = false;
				bTraversalSent = true;
				SendInput(Winner, KnightGameplayTags::Input_World_Traverse);
			}
			MaximumTraversalZ = FMath::Max(MaximumTraversalZ, Pawn->GetActorLocation().Z);
			MaximumOwnerTraversalZ = FMath::Max(MaximumOwnerTraversalZ, Replica->GetActorLocation().Z);
			if (Traversal->HasActiveTraversal())
			{
				bTraversalStarted = true;
				CompletedTraversalHandle = Traversal->GetSnapshot().Handle;
				Landing = Traversal->GetSnapshot().Landing.GetLocation();
				UAnimMontage* Montage = Pawn->GetKnightAbilitySystem()->GetCurrentMontage();
				bOwnerTraversalMontage = bOwnerTraversalMontage || (Montage != nullptr
					&& Replica->GetKnightAbilitySystem()->GetCurrentMontage() == Montage
					&& Replica->GetMesh()->GetAnimInstance()->Montage_IsPlaying(Montage));
				return;
			}
			if (!bTraversalStarted || !Pawn->GetCharacterMovement()->IsMovingOnGround()
				|| Pawn->GetCameraModeComponent()->GetModeCount() != 0
				|| Clients[Winner]->GetControlledKnight()->GetCameraModeComponent()->GetModeCount() != 0)
			{
				return;
			}
			if (!bOwnerTraversalMontage || MaximumTraversalZ < TraversalStart.Z + RequiredTraversalArc
				|| Pawn->GetActorLocation().X < -960.0f
				|| FVector::Dist(Pawn->GetActorLocation(), Landing) > 100.0f
				|| Pawn->GetCombatComponent()->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::Traversal) != 1
				|| Pawn->FindComponentByClass<UMotionWarpingComponent>()->GetWarpTargets().Num() != 0
				|| !HasInactiveAbilityInputs(Pawn->GetKnightAbilitySystem())
				|| Clients[Winner]->IsMoveInputIgnored() || Clients[Winner]->IsLookInputIgnored())
			{
				Fail(GetTraversalDiagnostics(TEXT("Authoritative traversal failed arc/landing/outcome/cleanup")));
				return;
			}
			Enter(EKnightCombatLifecyclePhase::TraverseConverge);
		}
		void WaitTraverseConverge()
		{
			AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			AKnightCharacter* Replica = Clients[Winner]->GetControlledKnight();
			if (Replica == nullptr)
			{
				Fail(GetTraversalDiagnostics(TEXT("Owning traversal pawn was lost during convergence")));
				return;
			}
			MaximumOwnerTraversalZ = FMath::Max(MaximumOwnerTraversalZ, Replica->GetActorLocation().Z);
			const bool bOwnerConverged = MaximumOwnerTraversalZ >= OwnerTraversalStart.Z + RequiredTraversalArc
				&& Replica->GetCharacterMovement()->MovementMode == MOVE_Walking
				&& FVector::Dist(Replica->GetActorLocation(), Pawn->GetActorLocation()) <= OwnerLandingTolerance
				&& FVector::Dist(Replica->GetActorLocation(), Landing) <= OwnerLandingTolerance
				&& !Replica->FindComponentByClass<UKnightTraversalComponent>()->HasActiveTraversal()
				&& Replica->FindComponentByClass<UMotionWarpingComponent>()->GetWarpTargets().Num() == 0
				&& !Replica->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Traversing)
				&& Replica->GetCameraModeComponent()->GetModeCount() == 0
				&& !Clients[Winner]->IsMoveInputIgnored() && !Clients[Winner]->IsLookInputIgnored();
			if (!bOwnerConverged)
			{
				if (Elapsed() > OwnerConvergenceTimeoutSeconds)
				{
					Fail(GetTraversalDiagnostics(TEXT("Owning client failed physical arc/landing/Walking convergence within 5s")));
				}
				return;
			}
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			const FVector CompletedPosition = Pawn->GetActorLocation();
			Pawn->FindComponentByClass<UKnightTraversalComponent>()->EndTraversal(CompletedTraversalHandle, false);
			if (!CompletedTraversalHandle.IsValid()
				|| Pawn->FindComponentByClass<UKnightTraversalComponent>()->HasActiveTraversal()
				|| Pawn->GetCombatComponent()->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::Traversal) != 1
				|| !HasInactiveAbilityInputs(Pawn->GetKnightAbilitySystem())
				|| Pawn->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Traversing)
				|| Pawn->FindComponentByClass<UMotionWarpingComponent>()->GetWarpTargets().Num() != 0
				|| Pawn->GetCameraModeComponent()->GetModeCount() != 0
				|| Pawn->GetCharacterMovement()->MovementMode != MOVE_Walking
				|| !Pawn->GetActorLocation().Equals(CompletedPosition, 0.01f)
				|| Clients[Winner]->IsMoveInputIgnored() || Clients[Winner]->IsLookInputIgnored())
			{
				Fail(GetTraversalDiagnostics(TEXT("Stale completed traversal handle mutated outcome or restored ownership state")));
				return;
			}
			Test.AddInfo(GetTraversalDiagnostics(TEXT("Real server and owning-client traversal crossed the hurdle")));
			Test.AddInfo(TEXT("One authoritative completion without completion teleport; owning client observed montage, "
				"upward arc, physical landing and Walking, with camera/input/warp targets restored under 50–100 ms latency."));
			Test.AddInfo(TEXT("Natural traversal ended its GAS spec; its stale handle left outcome, movement and camera/input unchanged."));
			BeginCleanup();
		}
		FString GetTraversalDiagnostics(const TCHAR* Context) const
		{
			const AKnightCharacter* Pawn = Knights[Slots[Winner]].Get();
			const AKnightCharacter* Replica = Clients[Winner]->GetControlledKnight();
			const FString ServerPosition = Pawn != nullptr ? Pawn->GetActorLocation().ToString() : TEXT("<missing>");
			const FString OwnerPosition = Replica != nullptr ? Replica->GetActorLocation().ToString() : TEXT("<missing>");
			return FString::Printf(TEXT("%s: accepted=%s serverStart=%s ownerStart=%s landing=%s server=%s owner=%s "
				"serverMode=%d ownerMode=%d serverMaxZ=%.1f ownerMaxZ=%.1f separation=%.1f serverInputsInactive=%d."),
				Context, *CompletedTraversalHandle.ToString(), *TraversalStart.ToString(), *OwnerTraversalStart.ToString(),
				*Landing.ToString(),
				*ServerPosition, *OwnerPosition,
				Pawn != nullptr ? static_cast<int32>(Pawn->GetCharacterMovement()->MovementMode) : INDEX_NONE,
				Replica != nullptr ? static_cast<int32>(Replica->GetCharacterMovement()->MovementMode) : INDEX_NONE,
				MaximumTraversalZ, MaximumOwnerTraversalZ,
				Pawn != nullptr && Replica != nullptr
					? FVector::Dist(Pawn->GetActorLocation(), Replica->GetActorLocation()) : -1.0f,
				Pawn != nullptr && HasInactiveAbilityInputs(Pawn->GetKnightAbilitySystem()));
		}

		void ReleaseExternalLocks()
		{
			if (bExternalLocks && ExternalLockOwner.IsValid())
			{
				ExternalLockOwner->SetIgnoreMoveInput(false);
				ExternalLockOwner->SetIgnoreLookInput(false);
			}
			bExternalLocks = false;
			ExternalLockOwner.Reset();
		}
		void BeginCleanup()
		{
			if (Phase >= EKnightCombatLifecyclePhase::Cleanup)
			{
				return;
			}
			ReleaseExternalLocks();
			Enter(EKnightCombatLifecyclePhase::Cleanup);
			if (bOwnsSession && GEditor != nullptr)
			{
				if (GEditor->IsPlayingSessionInEditor())
				{
					GEditor->RequestEndPlayMap();
				}
				else if (GEditor->IsPlaySessionRequestQueued())
				{
					GEditor->CancelRequestPlaySession();
				}
			}
		}
		void RestoreWindows()
		{
			if (Settings.IsValid())
			{
				ULevelEditorPlaySettings* Defaults = GetMutableDefault<ULevelEditorPlaySettings>();
				Defaults->MultipleInstancePositions = OriginalPositions;
				Defaults->LastSize = OriginalLastSize;
				Defaults->NewWindowPosition = OriginalWindowPosition;
				Defaults->NewWindowWidth = OriginalWidth;
				Defaults->NewWindowHeight = OriginalHeight;
				Defaults->SaveConfig();
			}
		}
		void Cleanup()
		{
			if (bOwnsSession && GEditor != nullptr && GEditor->IsPlaySessionInProgress())
			{
				if (Elapsed() < 15.0)
				{
					return;
				}
				Test.AddError(TEXT("Combat network PIE cleanup timed out; forcing synchronous EndPlay."));
				if (GEditor->IsPlayingSessionInEditor())
				{
					GEditor->EndPlayMap();
				}
				else if (GEditor->IsPlaySessionRequestQueued())
				{
					GEditor->CancelRequestPlaySession();
				}
			}
			RestoreWindows();
			Settings.Reset();
			bOwnsSession = false;
			Enter(EKnightCombatLifecyclePhase::Complete);
		}

		FAutomationTestBase& Test;
		EKnightCombatLifecyclePhase Phase = EKnightCombatLifecyclePhase::Start;
		double PhaseStarted = 0.0;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize;
		FIntPoint OriginalWindowPosition;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		TWeakObjectPtr<UWorld> Server;
		TArray<FName> ClientContexts;
		TArray<TWeakObjectPtr<AKnightPlayerController>> Clients;
		TArray<int32> Slots;
		TArray<TWeakObjectPtr<AKnightCharacter>> Knights;
		TArray<FKnightEntityId> Ids;
		TArray<FPrimaryAssetId> Heroes;
		TArray<TArray<FGameplayAbilitySpecHandle>> Specs;
		TArray<int32> Grants;
		TWeakObjectPtr<AKnightCombatFixture> Target;
		TWeakObjectPtr<AKnightPlayerController> ExternalLockOwner;
		FKnightResult HostRetryResult;
		FGuid CompletedFinisherHandle;
		FGuid CompletedTraversalHandle;
		FString ReconnectURL;
		int32 Winner = INDEX_NONE;
		float TargetHealth = 0.0f;
		FVector TraversalStart = FVector::ZeroVector;
		FVector OwnerTraversalStart = FVector::ZeroVector;
		FVector Landing = FVector::ZeroVector;
		double MaximumTraversalZ = 0.0;
		double MaximumOwnerTraversalZ = 0.0;
		bool bOwnsSession = false;
		bool bExternalLocks = false;
		bool bTraversalSent = false;
		bool bTraversalStarted = false;
		bool bOwnerTraversalMontage = false;
		bool bNaturalFinisherObserved = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightCombatLifecycleIntegrationTest, "Knight.Combat.NetworkInterruptTraversal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightCombatLifecycleIntegrationTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<KnightCombatLifecycle::FKnightCombatLifecycleCommand>(*this));
	return true;
}

#endif
