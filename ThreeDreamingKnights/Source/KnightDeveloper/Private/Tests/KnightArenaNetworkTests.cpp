#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AIController.h"
#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Combat/KnightCombatDirectorSubsystem.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightAIDecisionSchedulerComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/TextRenderComponent.h"
#include "Context/KnightAIContextComponent.h"
#include "Controllers/KnightArenaCompanionController.h"
#include "Controllers/KnightEnemyController.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/KnightGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayAbilitySpec.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Misc/PackageName.h"
#include "Party/KnightPartyState.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "PartyArena/KnightArenaPlayerController.h"
#include "PartyArena/KnightPartyArenaGameMode.h"
#include "PartyArena/KnightPartyArenaStartMarker.h"
#include "PartyArena/KnightPartyArenaState.h"
#include "Perception/AISense_Sight.h"
#include "Perception/KnightAIRolePerceptionComponent.h"
#include "PlayInEditorDataTypes.h"
#include "Player/KnightPlayerState.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tags/KnightGameplayTags.h"
#include "Threat/KnightThreatComponent.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace KnightArenaNetwork
{
	const TCHAR* ArenaMap = TEXT("/Game/Knight/Maps/Playables/L_M04_PartyArena");
	const TCHAR* DetachedMap = TEXT("/Game/Knight/Maps/Playables/L_M00_Boot");
	constexpr double StartupTimeout = 60.0;
	constexpr double PhaseTimeout = 25.0;
	constexpr uint16 ServerPort = 17904;

	enum class EKnightArenaNetworkPhase : uint8
	{
		Start, Ready, Disconnect, Hold, StaleKnight, ForeignLeader, StaleFocus, FriendlyFocus,
		StartWave, SightAcquired, SightLost, SightRestored, Focus, Attack, CancelAttack,
		Revive, Rejoin, OwnerRevive, CompleteRevive, DestroyTarget, Cleanup, Complete
	};

	struct FKnightArenaNetworkCapture
	{
		int32 ReplyCount = 0;
		FKnightResult Reply;
		TMap<FGuid, EKnightAIActionStatus> FinishedActions;
		TMap<FGuid, EKnightAttackLeaseReleaseReason> ReleasedLeases;
		int64 DecisionTickCount = 0;
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

	bool HasInactiveInputs(const UKnightAbilitySystemComponent* ASC)
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.IsActive() || Spec.InputPressed)
			{
				return false;
			}
		}
		return true;
	}

	/** Positions isolate ownership faults; action, perception, revive, RPCs and network travel remain real. */
	class FKnightArenaNetworkCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FKnightArenaNetworkCommand(FAutomationTestBase& InTest)
			: Test(InTest), Capture(MakeShared<FKnightArenaNetworkCapture>())
		{
		}

		virtual ~FKnightArenaNetworkCommand() override
		{
			UnbindObservers();
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
			if (Phase < EKnightArenaNetworkPhase::Cleanup && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			if (Phase > EKnightArenaNetworkPhase::Ready && Phase < EKnightArenaNetworkPhase::Cleanup
				&& !HasCanonicalIdentities())
			{
				Fail(TEXT("Arena network mutation changed canonical actors, stable IDs, ASC ownership or exact grants."));
			}
			if (Phase > EKnightArenaNetworkPhase::Start && Phase < EKnightArenaNetworkPhase::Cleanup
				&& Elapsed() > (Phase == EKnightArenaNetworkPhase::Ready ? StartupTimeout : PhaseTimeout))
			{
				Fail(GetDiagnostics(TEXT("Actual arena network phase timed out")));
			}
			switch (Phase)
			{
			case EKnightArenaNetworkPhase::Start: Start(); break;
			case EKnightArenaNetworkPhase::Ready: WaitReady(); break;
			case EKnightArenaNetworkPhase::Disconnect: WaitDisconnect(); break;
			case EKnightArenaNetworkPhase::Hold: WaitHold(); break;
			case EKnightArenaNetworkPhase::StaleKnight: WaitStaleKnight(); break;
			case EKnightArenaNetworkPhase::ForeignLeader: WaitForeignLeader(); break;
			case EKnightArenaNetworkPhase::StaleFocus: WaitStaleFocus(); break;
			case EKnightArenaNetworkPhase::FriendlyFocus: WaitFriendlyFocus(); break;
			case EKnightArenaNetworkPhase::StartWave: WaitStartWave(); break;
			case EKnightArenaNetworkPhase::SightAcquired: WaitSightAcquired(); break;
			case EKnightArenaNetworkPhase::SightLost: WaitSightLost(); break;
			case EKnightArenaNetworkPhase::SightRestored: WaitSightRestored(); break;
			case EKnightArenaNetworkPhase::Focus: WaitFocus(); break;
			case EKnightArenaNetworkPhase::Attack: WaitAttack(); break;
			case EKnightArenaNetworkPhase::CancelAttack: WaitCancelAttack(); break;
			case EKnightArenaNetworkPhase::Revive: WaitRevive(); break;
			case EKnightArenaNetworkPhase::Rejoin: WaitRejoin(); break;
			case EKnightArenaNetworkPhase::OwnerRevive: WaitOwnerRevive(); break;
			case EKnightArenaNetworkPhase::CompleteRevive: WaitCompleteRevive(); break;
			case EKnightArenaNetworkPhase::DestroyTarget: WaitDestroyedTarget(); break;
			case EKnightArenaNetworkPhase::Cleanup: Cleanup(); break;
			default: break;
			}
			return Phase == EKnightArenaNetworkPhase::Complete;
		}

	private:
		void Enter(EKnightArenaNetworkPhase InPhase)
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

		AKnightArenaPlayerController* FindOwner(UWorld* World) const
		{
			if (World == nullptr || World->GetNetMode() != NM_Client)
			{
				return nullptr;
			}
			AKnightPartyState* Party = FindParty(World);
			for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AKnightArenaPlayerController* Player = Cast<AKnightArenaPlayerController>(Iterator->Get());
				AKnightCharacter* Pawn = Player != nullptr ? Player->GetControlledKnight() : nullptr;
				AKnightPlayerState* Human = Player != nullptr ? Player->GetPlayerState<AKnightPlayerState>() : nullptr;
				const FKnightRosterEntry* Entry = Party != nullptr && Human != nullptr
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
				|| !FPackageName::DoesPackageExist(ArenaMap) || !FPackageName::DoesPackageExist(DetachedMap))
			{
				Fail(TEXT("Arena network gate requires an idle Editor and actual M0/M4 bootstrap maps."));
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
			Request.GlobalMapOverride = ArenaMap;
			Request.bAllowOnlineSubsystem = false;
			GEditor->RequestPlaySession(Request);
			bOwnsSession = true;
			Test.AddInfo(TEXT("Real M4 listen-server/two-client gate requested with outgoing 50–100ms packet latency."));
			Enter(EKnightArenaNetworkPhase::Ready);
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
			AKnightPartyArenaGameMode* Mode = Server.IsValid() ? Server->GetAuthGameMode<AKnightPartyArenaGameMode>() : nullptr;
			if (Party == nullptr || !Party->IsCanonicalRosterReady() || Party->GetHumanCount() != 3
				|| ClientContexts.Num() != 2 || Mode == nullptr || !Mode->GetArenaInitializationResult().IsSuccess())
			{
				return;
			}
			for (FName Handle : ClientContexts)
			{
				if (FindOwner(ContextWorld(Handle)) == nullptr)
				{
					return;
				}
			}
#if DO_ENABLE_NET_TEST
			if (Server->GetNetDriver() == nullptr || Server->GetNetDriver()->PacketSimulationSettings.PktLagMin != 50
				|| Server->GetNetDriver()->PacketSimulationSettings.PktLagMax != 100)
			{
				Fail(TEXT("Real arena NetDriver did not apply requested 50–100ms latency."));
				return;
			}
#else
			Fail(TEXT("This build cannot execute the required actual network packet latency gate."));
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
				if (Grants.Last() < 2 || Knight->GetCombatComponent()->GetProfile() == nullptr)
				{
					Fail(TEXT("Arena did not compose actual foundation and combat source grants."));
					return;
				}
			}
			DisconnectSlot = FindOwner(ContextWorld(ClientContexts[0]))->GetPlayerState<AKnightPlayerState>()->GetAssignedSlot();
			Requester = FindOwner(ContextWorld(ClientContexts[1]));
			RequesterSlot = Requester->GetPlayerState<AKnightPlayerState>()->GetAssignedSlot();
			for (FConstPlayerControllerIterator Iterator = Server->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AKnightArenaPlayerController* Candidate = Cast<AKnightArenaPlayerController>(Iterator->Get());
				if (Candidate != nullptr && Candidate->IsLocalController())
				{
					Host = Candidate;
					HostSlot = Candidate->GetPlayerState<AKnightPlayerState>()->GetAssignedSlot();
				}
			}
			if (!Host.IsValid())
			{
				Fail(TEXT("Actual listen-server host controller is unavailable."));
				return;
			}
			const TWeakPtr<FKnightArenaNetworkCapture> WeakCapture(Capture);
			ReplyHandle = Requester->OnCompanionCommandResult.AddLambda([WeakCapture](const FKnightResult& Result)
				{
					if (TSharedPtr<FKnightArenaNetworkCapture> Current = WeakCapture.Pin())
					{
						++Current->ReplyCount;
						Current->Reply = Result;
					}
				});
			const TSharedPtr<const FInternetAddr> Address = Server->GetNetDriver()->GetLocalAddr();
			if (!Address.IsValid())
			{
				Fail(TEXT("Actual listen server has no bound address for reconnect."));
				return;
			}
			ReconnectURL = FString::Printf(TEXT("127.0.0.1:%d?KnightSlot=%d"), Address->GetPort(), DisconnectSlot);
			AKnightArenaPlayerController* Departing = FindOwner(ContextWorld(ClientContexts[0]));
			FScopedConditionalWorldSwitcher Switch(Departing->GetWorld());
			Departing->ClientTravel(DetachedMap, TRAVEL_Absolute);
			Enter(EKnightArenaNetworkPhase::Disconnect);
		}

		bool HasCanonicalIdentities() const
		{
			AKnightPartyState* Party = FindParty(Server.Get());
			if (Party == nullptr || Knights.Num() != 3)
			{
				return false;
			}
			int32 Count = 0;
			for (TActorIterator<AKnightCharacter> Iterator(Server.Get()); Iterator; ++Iterator)
			{
				++Count;
			}
			if (Count != 3)
			{
				return false;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				if (!Knights[Slot].IsValid() || Entry == nullptr || Entry->Knight != Knights[Slot].Get()
					|| Entry->KnightId != Ids[Slot] || Entry->HeroDefinitionId != Heroes[Slot]
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

		void Place(ACharacter* Pawn, const FVector& Location)
		{
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Pawn->GetCharacterMovement()->StopMovementImmediately();
			Pawn->SetActorLocationAndRotation(Location, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
			Pawn->ForceNetUpdate();
		}

		bool SendCommand(EKnightCompanionCommand Command, const FKnightEntityId& ExpectedId,
			const FKnightEntityId& FocusId = FKnightEntityId())
		{
			UFunction* Function = Requester->FindFunction(TEXT("ServerRequestCompanionCommand"));
			struct FKnightCommandParams
			{
				EKnightCompanionCommand Command;
				FKnightEntityId ExpectedKnightId;
				FKnightEntityId FocusId;
			};
			if (Function == nullptr || !Function->HasAnyFunctionFlags(FUNC_NetServer)
				|| Function->ParmsSize != sizeof(FKnightCommandParams))
			{
				Fail(TEXT("Native command RPC reflection contract changed; no direct implementation bypass is allowed."));
				return false;
			}
			PreviousReplies = Capture->ReplyCount;
			FKnightCommandParams Parameters{Command, ExpectedId, FocusId};
			FScopedConditionalWorldSwitcher Switch(Requester->GetWorld());
			Requester->ProcessEvent(Function, &Parameters);
			return true;
		}

		void WaitDisconnect()
		{
			UWorld* Detached = ContextWorld(ClientContexts[0]);
			const FKnightRosterEntry* Entry = FindParty(Server.Get())->FindSlot(DisconnectSlot);
			Companion = Cast<AKnightArenaCompanionController>(Knights[DisconnectSlot]->GetController());
			if (Detached == nullptr || Detached->GetNetMode() != NM_Standalone || Detached->GetNetDriver() != nullptr
				|| Detached->GetFirstPlayerController() == nullptr || Entry->OwnerKind != EKnightControllerOwner::Companion
				|| !Companion.IsValid() || !Companion->GetArenaInitializationResult().IsSuccess()
				|| !Companion->IsFollowing() || !Companion->GetDecisionScheduler()->HasDecisionTimer())
			{
				return;
			}
			Place(Knights[DisconnectSlot].Get(), FVector(-1000.0, -500.0, 100.0));
			Place(Knights[RequesterSlot].Get(), FVector(-900.0, -700.0, 100.0));
			Place(Knights[HostSlot].Get(), FVector(900.0, -900.0, 100.0));
			SourceThreat = Knights[DisconnectSlot]->FindComponentByClass<UKnightThreatComponent>();
			if (!SourceThreat.IsValid() || !SourceThreat->TrySetThreatTuning(0.0f, 1.0f).IsSuccess())
			{
				Fail(TEXT("Ownership fixture requires the native empty threat table with decay disabled to isolate takeover."));
				return;
			}
			const TWeakPtr<FKnightArenaNetworkCapture> WeakCapture(Capture);
			ObservedContext = Companion->GetAIContext();
			ObservedScheduler = Companion->GetDecisionScheduler();
			ActionHandle = ObservedContext->OnActionFinished.AddLambda(
				[WeakCapture](FGuid Token, EKnightAIActionStatus Status)
				{
					if (TSharedPtr<FKnightArenaNetworkCapture> Current = WeakCapture.Pin())
					{
						Current->FinishedActions.Add(Token, Status);
					}
				});
			ScheduleHandle = ObservedScheduler->OnDecisionScheduled.AddLambda(
				[WeakCapture](EKnightAILODTier Tier, float Interval, int64 TickCount)
				{
					if (TSharedPtr<FKnightArenaNetworkCapture> Current = WeakCapture.Pin())
					{
						Current->DecisionTickCount = TickCount;
					}
				});
			Director = Server->GetSubsystem<UKnightCombatDirectorSubsystem>();
			LeaseHandle = Director->OnAttackLeaseReleased.AddLambda(
				[WeakCapture](const FKnightAttackLease& Lease, EKnightAttackLeaseReleaseReason Reason)
				{
					if (TSharedPtr<FKnightArenaNetworkCapture> Current = WeakCapture.Pin())
					{
						Current->ReleasedLeases.Add(Lease.LeaseId, Reason);
					}
				});
			Test.AddInfo(TEXT("Real disconnect preserved the same Knight and started its native arena StateTree/scheduler."));
			if (SendCommand(EKnightCompanionCommand::Hold, Ids[RequesterSlot]))
			{
				Enter(EKnightArenaNetworkPhase::Hold);
			}
		}

		void WaitHold()
		{
			if (Capture->ReplyCount <= PreviousReplies)
			{
				return;
			}
			if (!Capture->Reply.IsSuccess() || Companion->GetCommandSnapshot().Command != EKnightCompanionCommand::Hold
				|| !Companion->GetAIContext()->GetDecisionSnapshot().bHoldPosition)
			{
				Fail(GetDiagnostics(TEXT("Real owned command did not enter Hold")));
				return;
			}
			CommandRevision = Companion->GetCommandSnapshot().CommandRevision;
			if (SendCommand(EKnightCompanionCommand::Regroup, Ids[HostSlot]))
			{
				Enter(EKnightArenaNetworkPhase::StaleKnight);
			}
		}

		bool HasRejectedCommand(EKnightError ExpectedError)
		{
			if (Capture->ReplyCount <= PreviousReplies)
			{
				return false;
			}
			if (Capture->Reply.Error != ExpectedError || Companion->GetCommandSnapshot().CommandRevision != CommandRevision
				|| Companion->GetCommandSnapshot().Command != EKnightCompanionCommand::Hold)
			{
				Fail(GetDiagnostics(TEXT("Rejected RPC mutated command state or returned the wrong typed failure")));
				return false;
			}
			return true;
		}

		void WaitStaleKnight()
		{
			if (HasRejectedCommand(EKnightError::NotOwned))
			{
				Place(Knights[DisconnectSlot].Get(), FVector(1000.0, -900.0, 100.0));
				if (SendCommand(EKnightCompanionCommand::Regroup, Ids[RequesterSlot]))
				{
					Enter(EKnightArenaNetworkPhase::ForeignLeader);
				}
			}
		}

		void WaitForeignLeader()
		{
			if (HasRejectedCommand(EKnightError::NotFound))
			{
				Place(Knights[DisconnectSlot].Get(), FVector(-1000.0, -500.0, 100.0));
				if (SendCommand(EKnightCompanionCommand::Focus, Ids[RequesterSlot], FKnightEntityId::Create()))
				{
					Enter(EKnightArenaNetworkPhase::StaleFocus);
				}
			}
		}

		void WaitStaleFocus()
		{
			if (HasRejectedCommand(EKnightError::InvalidId)
				&& SendCommand(EKnightCompanionCommand::Focus, Ids[RequesterSlot], Ids[HostSlot]))
			{
				Enter(EKnightArenaNetworkPhase::FriendlyFocus);
			}
		}

		void WaitFriendlyFocus()
		{
			if (!HasRejectedCommand(EKnightError::InvalidId))
			{
				return;
			}
			AKnightPartyArenaStartMarker* Marker = nullptr;
			for (TActorIterator<AKnightPartyArenaStartMarker> Iterator(Server.Get()); Iterator; ++Iterator)
			{
				Marker = *Iterator;
				break;
			}
			if (Marker == nullptr)
			{
				Fail(TEXT("Actual M4 start interaction marker is missing."));
				return;
			}
			Test.AddInfo(TEXT("Real RPC rejects stale Knight, another human's follower, stale focus ID and friendly ID without mutation."));
			Place(Knights[HostSlot].Get(), Marker->GetActorLocation() + FVector(0.0, -150.0, 100.0));
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Host->RequestInteract();
			Enter(EKnightArenaNetworkPhase::StartWave);
		}

		void WaitStartWave()
		{
			AKnightPartyArenaGameMode* Mode = Server->GetAuthGameMode<AKnightPartyArenaGameMode>();
			if (Mode->GetArenaState()->GetSnapshot().Stage != EKnightArenaStage::Wave || Mode->GetEncounterEnemies().IsEmpty())
			{
				return;
			}
			for (AKnightArenaEnemy* Enemy : Mode->GetEncounterEnemies())
			{
				AKnightEnemyController* Controller = Cast<AKnightEnemyController>(Enemy->GetController());
				if (Controller == nullptr || !Controller->GetInitializationResult().IsSuccess())
				{
					Fail(TEXT("Actual wave enemy controller/archetype failed before network action isolation."));
					return;
				}
				// Isolate incoming attacks so exact persistent health can be checked across possession.
				Controller->GetDecisionScheduler()->StopScheduling();
				Controller->GetDecisionTree()->StopLogic(TEXT("Network ownership fault fixture"));
				Controller->GetAIContext()->CancelOwnedAction();
				Controller->StopMovement();
			}
			Target = Mode->GetEncounterEnemies()[0];
			for (int32 Index = 1; Index < Mode->GetEncounterEnemies().Num(); ++Index)
			{
				Place(Mode->GetEncounterEnemies()[Index], FVector(1800.0, 1700.0 - Index * 120.0, 100.0));
			}
			Place(Target.Get(), FVector(-900.0, 0.0, 100.0));
			Place(Knights[DisconnectSlot].Get(), FVector(-1080.0, 0.0, 100.0));
			Place(Knights[RequesterSlot].Get(), FVector(-1080.0, -350.0, 100.0));
			Place(Knights[HostSlot].Get(), FVector(1200.0, -1200.0, 100.0));
			Enter(EKnightArenaNetworkPhase::SightAcquired);
		}

		bool IsTargetActuallySeen() const
		{
			TArray<AActor*> Perceived;
			Companion->GetRolePerception()->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Perceived);
			return Perceived.Contains(Target.Get());
		}

		void WaitSightAcquired()
		{
			if (!IsTargetActuallySeen() || Companion->GetRolePerception()->GetStimulusUpdateCount() < 1
				|| Companion->GetAIContext()->GetDecisionSnapshot().TargetId != Target->GetArenaEnemyId()
				|| SourceThreat->GetThreat(Target->GetArenaEnemyId()) <= 0.0f)
			{
				return;
			}
			SightUpdates = Companion->GetRolePerception()->GetStimulusUpdateCount();
			Place(Target.Get(), FVector(1800.0, -1700.0, 100.0));
			Enter(EKnightArenaNetworkPhase::SightLost);
		}

		void WaitSightLost()
		{
			if (IsTargetActuallySeen() || Companion->GetRolePerception()->GetStimulusUpdateCount() <= SightUpdates
				|| Companion->GetAIContext()->GetDecisionSnapshot().TargetId == Target->GetArenaEnemyId()
				|| SourceThreat->GetThreat(Target->GetArenaEnemyId()) > 0.0f)
			{
				return;
			}
			Test.AddInfo(TEXT("Actual AIPerception sight acquisition/loss dropped the distant selected target and native observed threat."));
			Place(Target.Get(), FVector(-900.0, 0.0, 100.0));
			Enter(EKnightArenaNetworkPhase::SightRestored);
		}

		void WaitSightRestored()
		{
			if (!IsTargetActuallySeen())
			{
				return;
			}
			FScopedConditionalWorldSwitcher Switch(Requester->GetWorld());
			Requester->RequestAbilityInput(KnightGameplayTags::Input_Combat_LockOn, true);
			Requester->RequestAbilityInput(KnightGameplayTags::Input_Combat_LockOn, false);
			Enter(EKnightArenaNetworkPhase::Focus);
		}

		void WaitFocus()
		{
			AActor* ReplicaTarget = Requester->GetControlledKnight()->GetCombatComponent()->GetLockedTarget();
			UKnightAbilitySystemComponent* ReplicaASC = ReplicaTarget != nullptr
				? ReplicaTarget->FindComponentByClass<UKnightAbilitySystemComponent>() : nullptr;
			if (!bFocusSent)
			{
				if (ReplicaASC == nullptr || ReplicaASC->GetEntityId() != Target->GetArenaEnemyId())
				{
					return;
				}
				PreviousReplies = Capture->ReplyCount;
				FScopedConditionalWorldSwitcher Switch(Requester->GetWorld());
				Requester->RequestCompanionCommand(EKnightCompanionCommand::Focus);
				bFocusSent = true;
				return;
			}
			if (Capture->ReplyCount <= PreviousReplies)
			{
				return;
			}
			if (!Capture->Reply.IsSuccess() || Companion->GetCommandSnapshot().Command != EKnightCompanionCommand::Focus
				|| Companion->GetCommandSnapshot().FocusTargetId != Target->GetArenaEnemyId())
			{
				Fail(GetDiagnostics(TEXT("Real lock-derived Focus RPC failed")));
				return;
			}
			Enter(EKnightArenaNetworkPhase::Attack);
		}

		void WaitAttack()
		{
			const FKnightAIDecisionSnapshot Snapshot = Companion->GetAIContext()->GetDecisionSnapshot();
			const FKnightAttackLease Lease = Companion->GetAIContext()->GetActiveAttackLease();
			if (!Snapshot.bActionRunning || !Snapshot.ActiveActionToken.IsValid() || !Lease.IsValid()
				|| !Director->IsLeaseActive(Lease) || Companion->GetDecisionScheduler()->GetDecisionTickCount() < 1)
			{
				return;
			}
			if (Snapshot.TargetId != Target->GetArenaEnemyId() || Lease.SourceId != Ids[DisconnectSlot]
				|| Lease.TargetId != Target->GetArenaEnemyId() || Director->GetActiveLeaseCount() > Director->GetGlobalBudget()
				|| Director->GetActiveLeaseCountForTarget(Lease.TargetId) > Director->GetPerTargetBudget())
			{
				Fail(TEXT("Real StateTree GAS attack acquired an invalid or over-budget capability."));
				return;
			}
			AttackToken = Snapshot.ActiveActionToken;
			AttackLease = Lease;
			Test.AddInfo(FString::Printf(TEXT("Observed actual StateTree GAS action %s token=%s lease=%s, native decision ticks=%lld."),
				*Snapshot.ActiveActionTag.ToString(), *AttackToken.ToString(), *Lease.LeaseId.ToString(),
				Companion->GetDecisionScheduler()->GetDecisionTickCount()));
			if (SendCommand(EKnightCompanionCommand::Hold, Ids[RequesterSlot]))
			{
				Enter(EKnightArenaNetworkPhase::CancelAttack);
			}
		}

		void WaitCancelAttack()
		{
			if (Capture->ReplyCount <= PreviousReplies)
			{
				return;
			}
			const EKnightAIActionStatus* Status = Capture->FinishedActions.Find(AttackToken);
			if (!Capture->Reply.IsSuccess() || Status == nullptr || *Status != EKnightAIActionStatus::Cancelled
				|| Director->IsLeaseActive(AttackLease) || !Capture->ReleasedLeases.Contains(AttackLease.LeaseId))
			{
				Fail(GetDiagnostics(TEXT("Hold did not cancel the captured real GAS action and release its exact lease")));
				return;
			}
			Test.AddInfo(TEXT("Actual owning-client Hold canceled the captured StateTree action and exact director capability."));
			Place(Knights[RequesterSlot].Get(), Knights[DisconnectSlot]->GetActorLocation() + FVector(0.0, -160.0, 0.0));
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			FKnightResult Result = Knights[RequesterSlot]->GetLifeComponent()->TrySetLifeTuning(60.0f, 8.0f, 300.0f, 0.35f);
			if (Result.IsSuccess())
			{
				Result = UKnightAbilitySystemComponent::ApplyDamage(Target.Get(), Knights[DisconnectSlot].Get(),
					Knights[DisconnectSlot]->GetKnightAttributeSet()->GetDefense() + 10.0f);
			}
			if (Result.IsSuccess())
			{
				Result = UKnightAbilitySystemComponent::ApplyDamage(Target.Get(), Knights[RequesterSlot].Get(),
					Knights[RequesterSlot]->GetKnightAttributeSet()->GetMaxHealth()
						+ Knights[RequesterSlot]->GetKnightAttributeSet()->GetDefense() + 1.0f);
			}
			if (!Result.IsSuccess() || !Knights[RequesterSlot]->GetLifeComponent()->IsDowned())
			{
				Fail(TEXT("Validated GAS setup did not create the real friendly downed revive target."));
				return;
			}
			PersistentHealth = Knights[DisconnectSlot]->GetKnightAttributeSet()->GetHealth();
			PersistentThreat = SourceThreat->GetThreat(Target->GetArenaEnemyId());
			if (PersistentThreat <= 0.0f)
			{
				Fail(TEXT("Actual sight and confirmed GAS damage did not create persistent pawn threat."));
				return;
			}
			Enter(EKnightArenaNetworkPhase::Revive);
		}

		void WaitRevive()
		{
			UKnightAbilitySystemComponent* ASC = Knights[DisconnectSlot]->GetKnightAbilitySystem();
			UKnightLifeComponent* TargetLife = Knights[RequesterSlot]->GetLifeComponent();
			const FKnightAIDecisionSnapshot Snapshot = Companion->GetAIContext()->GetDecisionSnapshot();
			if (!TargetLife->IsReviveInProgress() || TargetLife->GetActiveReviver() != Knights[DisconnectSlot].Get()
				|| !Snapshot.bTargetFriendlyDowned || !Snapshot.bActionRunning
				|| !ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving)
				|| !ASC->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Revive))
			{
				return;
			}
			ReviveToken = Snapshot.ActiveActionToken;
			Test.AddInfo(FString::Printf(TEXT("Real companion shared-GAS revive active token=%s; same-slot JIP requested midchannel."),
				*ReviveToken.ToString()));
			UWorld* Detached = ContextWorld(ClientContexts[0]);
			FScopedConditionalWorldSwitcher Switch(Detached);
			Detached->GetFirstPlayerController()->ClientTravel(ReconnectURL, TRAVEL_Absolute);
			Enter(EKnightArenaNetworkPhase::Rejoin);
		}

		bool HasClientSnapshots() const
		{
			const AKnightPartyState* Authority = FindParty(Server.Get());
			for (FName Handle : ClientContexts)
			{
				UWorld* World = ContextWorld(Handle);
				const AKnightPartyState* Replica = FindParty(World);
				if (World == nullptr || World->GetNetMode() != NM_Client || Replica == nullptr
					|| !Replica->IsCanonicalRosterReady() || Replica->GetRosterRevision() != Authority->GetRosterRevision())
				{
					return false;
				}
				int32 Count = 0;
				for (TActorIterator<AKnightCharacter> Iterator(World); Iterator; ++Iterator)
				{
					++Count;
				}
				if (Count != 3)
				{
					return false;
				}
				for (TActorIterator<AAIController> Iterator(World); Iterator; ++Iterator)
				{
					return false;
				}
				for (int32 Slot = 0; Slot < 3; ++Slot)
				{
					const FKnightRosterEntry* Entry = Replica->FindSlot(Slot);
					UKnightAbilitySystemComponent* ASC = Entry != nullptr && Entry->Knight != nullptr
						? Entry->Knight->GetKnightAbilitySystem() : nullptr;
					if (ASC == nullptr || Entry->KnightId != Ids[Slot] || Entry->HeroDefinitionId != Heroes[Slot]
						|| ASC->GetOwnerActor() != Entry->Knight || ASC->GetAvatarActor() != Entry->Knight
						|| Entry->Knight->GetLifeComponent()->GetLifeState() != Knights[Slot]->GetLifeComponent()->GetLifeState()
						|| ASC->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Revive)
							!= Knights[Slot]->GetKnightAbilitySystem()->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Revive))
					{
						return false;
					}
				}
			}
			return true;
		}

		void WaitRejoin()
		{
			Reclaimed = FindOwner(ContextWorld(ClientContexts[0]));
			if (!Reclaimed.IsValid() || Reclaimed->GetPlayerState<AKnightPlayerState>()->GetAssignedSlot() != DisconnectSlot
				|| FindParty(Server.Get())->GetHumanCount() != 3 || !HasClientSnapshots())
			{
				return;
			}
			const EKnightAIActionStatus* Status = Capture->FinishedActions.Find(ReviveToken);
			UKnightAbilitySystemComponent* ASC = Knights[DisconnectSlot]->GetKnightAbilitySystem();
			UKnightLifeComponent* TargetLife = Knights[RequesterSlot]->GetLifeComponent();
			const bool bControllerClean = !Companion.IsValid() || (Companion->GetPawn() == nullptr && !Companion->IsFollowing());
			const bool bContextClean = !ObservedContext.IsValid() || (ObservedContext->GetControlledPawn() == nullptr
				&& !ObservedContext->GetActiveAttackLease().IsValid() && ObservedContext->GetActiveEQSRequestIds().IsEmpty());
			const bool bSchedulerClean = !ObservedScheduler.IsValid() || !ObservedScheduler->HasDecisionTimer();
			if (Status == nullptr || *Status != EKnightAIActionStatus::Cancelled || !TargetLife->IsDowned()
				|| TargetLife->IsReviveInProgress() || ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving)
				|| ASC->IsGameplayCueActive(KnightGameplayTags::GameplayCue_Revive) || !HasInactiveInputs(ASC)
				|| !bControllerClean || !bContextClean || !bSchedulerClean || Director->GetActiveLeaseCount() != 0
				|| GetSpecs(Reclaimed->GetControlledKnight()->GetKnightAbilitySystem()) != Specs[DisconnectSlot]
				|| !FMath::IsNearlyEqual(Knights[DisconnectSlot]->GetKnightAttributeSet()->GetHealth(), PersistentHealth)
				|| SourceThreat->GetThreat(Target->GetArenaEnemyId()) < PersistentThreat)
			{
				Fail(GetDiagnostics(TEXT("Same-slot JIP did not clean exact AI action/reservation/cue/tree/timer or preserve persistent state")));
				return;
			}
			AKnightPartyArenaGameMode* Mode = Server->GetAuthGameMode<AKnightPartyArenaGameMode>();
			for (FName Handle : ClientContexts)
			{
				UWorld* ClientWorld = ContextWorld(Handle);
				if (ClientWorld == nullptr || ClientWorld->GetNetMode() != NM_Client)
				{
					return;
				}
				for (AKnightArenaEnemy* Enemy : Mode->GetEncounterEnemies())
				{
					AKnightArenaEnemy* Replica = nullptr;
					for (TActorIterator<AKnightArenaEnemy> Iterator(ClientWorld); Iterator; ++Iterator)
					{
						if (Iterator->GetArenaEnemyId() == Enemy->GetArenaEnemyId())
						{
							Replica = *Iterator;
							break;
						}
					}
					if (Replica == nullptr || Replica->GetKnightAbilitySystem()->GetEntityId() != Enemy->GetArenaEnemyId()
						|| Replica->GetCombatComponent()->GetProfile() == nullptr)
					{
						return;
					}
					const UTextRenderComponent* Caption = Replica->FindComponentByClass<UTextRenderComponent>();
					if (Caption == nullptr || !Caption->Text.EqualTo(NSLOCTEXT("KnightArena", "EnemyLabel", "ARENA ENEMY")))
					{
						Fail(FString::Printf(TEXT("Ready client enemy %s in %s has incorrect native caption '%s'."),
							*Enemy->GetArenaEnemyId().Value.ToString(), *Handle.ToString(),
							Caption != nullptr ? *Caption->Text.ToString() : TEXT("<missing>")));
						return;
					}
				}
			}
			Test.AddInfo(TEXT("Both real client worlds display the native ARENA ENEMY caption for each identity-ready wave enemy."));
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			// AssignHumanSlot destroys the old AI. Record the stopped count after legitimate JIP-loading decisions.
			OldDecisionCount = Capture->DecisionTickCount;
			Test.AddInfo(TEXT("Real JIP canceled the captured AI revive; same actor/ASC/health/specs retained, no client AI or old timer/lease."));
			{
				FScopedConditionalWorldSwitcher OwnerSwitch(Reclaimed->GetWorld());
				Reclaimed->RequestAbilityInput(KnightGameplayTags::Input_Party_Revive, true);
			}
			Enter(EKnightArenaNetworkPhase::OwnerRevive);
		}

		void WaitOwnerRevive()
		{
			UKnightLifeComponent* Life = Knights[RequesterSlot]->GetLifeComponent();
			if (!Life->IsReviveInProgress() || Life->GetActiveReviver() != Knights[DisconnectSlot].Get()
				|| !Knights[DisconnectSlot]->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving))
			{
				return;
			}
			bObservedOwnerRevive = true;
			Enter(EKnightArenaNetworkPhase::CompleteRevive);
		}

		void WaitCompleteRevive()
		{
			if (!Knights[RequesterSlot]->GetLifeComponent()->IsAlive())
			{
				return;
			}
			if (!bReleasedOwnerInput)
			{
				FScopedConditionalWorldSwitcher Switch(Reclaimed->GetWorld());
				Reclaimed->RequestAbilityInput(KnightGameplayTags::Input_Party_Revive, false);
				bReleasedOwnerInput = true;
				return;
			}
			if (!HasClientSnapshots() || !HasInactiveInputs(Knights[DisconnectSlot]->GetKnightAbilitySystem())
				|| !HasInactiveInputs(Reclaimed->GetControlledKnight()->GetKnightAbilitySystem())
				|| Knights[RequesterSlot]->GetLifeComponent()->IsReviveInProgress())
			{
				return;
			}
			if (!bObservedOwnerRevive || Knights[RequesterSlot]->GetKnightAttributeSet()->GetHealth() <= 0.0f
				|| Capture->DecisionTickCount != OldDecisionCount)
			{
				Fail(TEXT("Reclaimed human did not complete real shared revive or old AI continued running."));
				return;
			}
			Test.AddInfo(TEXT("Reclaimed owning client completed real held-E GAS revive; both client life/cue snapshots converged, canonical party/grants intact."));
			DestroyedTargetId = Target->GetArenaEnemyId();
			ThreatEntriesBeforeDestroy = SourceThreat->GetTrackedEntryCount();
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			if (!Target->Destroy())
			{
				Fail(TEXT("Native enemy actor rejected explicit end-of-life fault fixture."));
				return;
			}
			Enter(EKnightArenaNetworkPhase::DestroyTarget);
		}

		void WaitDestroyedTarget()
		{
			if (SourceThreat->GetThreat(DestroyedTargetId) > 0.0f
				|| SourceThreat->GetTrackedEntryCount() != ThreatEntriesBeforeDestroy - 1)
			{
				return;
			}
			const FKnightPartyArenaSnapshot& ServerSnapshot = Server->GetAuthGameMode<AKnightPartyArenaGameMode>()
				->GetArenaState()->GetSnapshot();
			if (ServerSnapshot.Stage != EKnightArenaStage::Defeat
				|| ServerSnapshot.Failure != EKnightArenaFailure::EncounterUnavailable)
			{
				return;
			}
			for (FName Handle : ClientContexts)
			{
				AKnightPartyArenaState* Replica = nullptr;
				for (TActorIterator<AKnightPartyArenaState> Iterator(ContextWorld(Handle)); Iterator; ++Iterator)
				{
					Replica = *Iterator;
					break;
				}
				if (Replica == nullptr || Replica->GetSnapshot().Revision != ServerSnapshot.Revision
					|| Replica->GetSnapshot().Stage != EKnightArenaStage::Defeat
					|| Replica->GetSnapshot().Failure != EKnightArenaFailure::EncounterUnavailable)
				{
					return;
				}
			}
			Test.AddInfo(TEXT("Persistent confirmed pawn threat survived AI teardown and was removed by the real target EndPlay hook."));
			Test.AddInfo(TEXT("Unexpected living enemy destruction produced real EncounterUnavailable defeat on the server and both clients."));
			BeginCleanup();
		}

		FString GetDiagnostics(const TCHAR* Reason) const
		{
			const FKnightAIDecisionSnapshot Snapshot = Companion.IsValid()
				? Companion->GetAIContext()->GetDecisionSnapshot() : FKnightAIDecisionSnapshot();
			return FString::Printf(TEXT("%s: phase=%d, humans=%d, reply=%d/%d error=%d context=%s, companion=%d tree=%d timer=%d "
				"command=%d rev=%d action=%s token=%s lease=%s target=%s down=%d reservation=%d ticks=%lld."),
				Reason, static_cast<int32>(Phase), FindParty(Server.Get()) != nullptr ? FindParty(Server.Get())->GetHumanCount() : -1,
				Capture->ReplyCount, PreviousReplies, static_cast<int32>(Capture->Reply.Error), *Capture->Reply.Context.ToString(),
				Companion.IsValid(), Companion.IsValid() && Companion->IsFollowing(),
				Companion.IsValid() && Companion->GetDecisionScheduler()->HasDecisionTimer(),
				Companion.IsValid() ? static_cast<int32>(Companion->GetCommandSnapshot().Command) : -1,
				Companion.IsValid() ? Companion->GetCommandSnapshot().CommandRevision : -1,
				*Snapshot.ActiveActionTag.ToString(), *Snapshot.ActiveActionToken.ToString(),
				*AttackLease.LeaseId.ToString(), *Snapshot.TargetId.Value.ToString(),
				Knights.IsValidIndex(RequesterSlot) && Knights[RequesterSlot].IsValid()
					&& Knights[RequesterSlot]->GetLifeComponent()->IsDowned(),
				Knights.IsValidIndex(RequesterSlot) && Knights[RequesterSlot].IsValid()
					&& Knights[RequesterSlot]->GetLifeComponent()->IsReviveInProgress(),
				Companion.IsValid() ? Companion->GetDecisionScheduler()->GetDecisionTickCount() : -1);
		}

		void UnbindObservers()
		{
			if (Requester.IsValid())
			{
				Requester->OnCompanionCommandResult.Remove(ReplyHandle);
			}
			if (ObservedContext.IsValid())
			{
				ObservedContext->OnActionFinished.Remove(ActionHandle);
			}
			if (ObservedScheduler.IsValid())
			{
				ObservedScheduler->OnDecisionScheduled.Remove(ScheduleHandle);
			}
			if (Director.IsValid())
			{
				Director->OnAttackLeaseReleased.Remove(LeaseHandle);
			}
			ReplyHandle.Reset();
			ActionHandle.Reset();
			LeaseHandle.Reset();
			ScheduleHandle.Reset();
		}

		void BeginCleanup()
		{
			if (Phase >= EKnightArenaNetworkPhase::Cleanup)
			{
				return;
			}
			UnbindObservers();
			Enter(EKnightArenaNetworkPhase::Cleanup);
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
				Test.AddError(TEXT("Arena network PIE cleanup timed out; forcing synchronous EndPlay."));
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
			Enter(EKnightArenaNetworkPhase::Complete);
		}

		FAutomationTestBase& Test;
		TSharedPtr<FKnightArenaNetworkCapture> Capture;
		EKnightArenaNetworkPhase Phase = EKnightArenaNetworkPhase::Start;
		double PhaseStarted = 0.0;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize;
		FIntPoint OriginalWindowPosition;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		TWeakObjectPtr<UWorld> Server;
		TArray<FName> ClientContexts;
		TWeakObjectPtr<AKnightArenaPlayerController> Host;
		TWeakObjectPtr<AKnightArenaPlayerController> Requester;
		TWeakObjectPtr<AKnightArenaPlayerController> Reclaimed;
		TWeakObjectPtr<AKnightArenaCompanionController> Companion;
		TWeakObjectPtr<UKnightCombatDirectorSubsystem> Director;
		TWeakObjectPtr<UKnightAIContextComponent> ObservedContext;
		TWeakObjectPtr<UKnightAIDecisionSchedulerComponent> ObservedScheduler;
		TWeakObjectPtr<UKnightThreatComponent> SourceThreat;
		TWeakObjectPtr<AKnightArenaEnemy> Target;
		TArray<TWeakObjectPtr<AKnightCharacter>> Knights;
		TArray<FKnightEntityId> Ids;
		TArray<FPrimaryAssetId> Heroes;
		TArray<TArray<FGameplayAbilitySpecHandle>> Specs;
		TArray<int32> Grants;
		FDelegateHandle ReplyHandle;
		FDelegateHandle ActionHandle;
		FDelegateHandle LeaseHandle;
		FDelegateHandle ScheduleHandle;
		FString ReconnectURL;
		FKnightAttackLease AttackLease;
		FGuid AttackToken;
		FGuid ReviveToken;
		FKnightEntityId DestroyedTargetId;
		int32 DisconnectSlot = INDEX_NONE;
		int32 RequesterSlot = INDEX_NONE;
		int32 HostSlot = INDEX_NONE;
		int32 CommandRevision = 0;
		int32 PreviousReplies = 0;
		int32 ThreatEntriesBeforeDestroy = 0;
		int64 OldDecisionCount = 0;
		int64 SightUpdates = 0;
		float PersistentHealth = 0.0f;
		float PersistentThreat = 0.0f;
		bool bOwnsSession = false;
		bool bFocusSent = false;
		bool bObservedOwnerRevive = false;
		bool bReleasedOwnerInput = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightArenaNetworkIntegrationTest, "Knight.AI.Arena.NetworkCommandsTakeoverJIP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightArenaNetworkIntegrationTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<KnightArenaNetwork::FKnightArenaNetworkCommand>(*this));
	return true;
}

#endif
