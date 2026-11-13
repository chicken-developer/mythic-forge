#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AIController.h"
#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "Combat/GolemCombatDirectorSubsystem.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemAIDecisionSchedulerComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/TextRenderComponent.h"
#include "Context/GolemAIContextComponent.h"
#include "Controllers/GolemArenaCompanionController.h"
#include "Controllers/GolemEnemyController.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/GolemGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayAbilitySpec.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Misc/PackageName.h"
#include "Party/GolemPartyState.h"
#include "PartyArena/GolemArenaEnemy.h"
#include "PartyArena/GolemArenaPlayerController.h"
#include "PartyArena/GolemPartyArenaGameMode.h"
#include "PartyArena/GolemPartyArenaStartMarker.h"
#include "PartyArena/GolemPartyArenaState.h"
#include "Perception/AISense_Sight.h"
#include "Perception/GolemAIRolePerceptionComponent.h"
#include "PlayInEditorDataTypes.h"
#include "Player/GolemPlayerState.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tags/GolemGameplayTags.h"
#include "Threat/GolemThreatComponent.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace GolemArenaNetwork
{
	const TCHAR* ArenaMap = TEXT("/Game/Golem/Maps/Playables/L_M04_PartyArena");
	const TCHAR* DetachedMap = TEXT("/Game/Golem/Maps/Playables/L_M00_Boot");
	constexpr double StartupTimeout = 60.0;
	constexpr double PhaseTimeout = 25.0;
	constexpr uint16 ServerPort = 17904;

	enum class EGolemArenaNetworkPhase : uint8
	{
		Start, Ready, Disconnect, Hold, StaleGolem, ForeignLeader, StaleFocus, FriendlyFocus,
		StartWave, SightAcquired, SightLost, SightRestored, Focus, Attack, CancelAttack,
		Revive, Rejoin, OwnerRevive, CompleteRevive, DestroyTarget, Cleanup, Complete
	};

	struct FGolemArenaNetworkCapture
	{
		int32 ReplyCount = 0;
		FGolemResult Reply;
		TMap<FGuid, EGolemAIActionStatus> FinishedActions;
		TMap<FGuid, EGolemAttackLeaseReleaseReason> ReleasedLeases;
		int64 DecisionTickCount = 0;
	};

	AGolemPartyState* FindParty(UWorld* World)
	{
		AGolemGameState* State = World != nullptr ? World->GetGameState<AGolemGameState>() : nullptr;
		return State != nullptr ? State->GetPartyState() : nullptr;
	}

	TArray<FGameplayAbilitySpecHandle> GetSpecs(const UGolemAbilitySystemComponent* ASC)
	{
		TArray<FGameplayAbilitySpecHandle> Result;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			Result.Add(Spec.Handle);
		}
		return Result;
	}

	bool HasInactiveInputs(const UGolemAbilitySystemComponent* ASC)
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
	class FGolemArenaNetworkCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FGolemArenaNetworkCommand(FAutomationTestBase& InTest)
			: Test(InTest), Capture(MakeShared<FGolemArenaNetworkCapture>())
		{
		}

		virtual ~FGolemArenaNetworkCommand() override
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
			if (Phase < EGolemArenaNetworkPhase::Cleanup && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			if (Phase > EGolemArenaNetworkPhase::Ready && Phase < EGolemArenaNetworkPhase::Cleanup
				&& !HasCanonicalIdentities())
			{
				Fail(TEXT("Arena network mutation changed canonical actors, stable IDs, ASC ownership or exact grants."));
			}
			if (Phase > EGolemArenaNetworkPhase::Start && Phase < EGolemArenaNetworkPhase::Cleanup
				&& Elapsed() > (Phase == EGolemArenaNetworkPhase::Ready ? StartupTimeout : PhaseTimeout))
			{
				Fail(GetDiagnostics(TEXT("Actual arena network phase timed out")));
			}
			switch (Phase)
			{
			case EGolemArenaNetworkPhase::Start: Start(); break;
			case EGolemArenaNetworkPhase::Ready: WaitReady(); break;
			case EGolemArenaNetworkPhase::Disconnect: WaitDisconnect(); break;
			case EGolemArenaNetworkPhase::Hold: WaitHold(); break;
			case EGolemArenaNetworkPhase::StaleGolem: WaitStaleGolem(); break;
			case EGolemArenaNetworkPhase::ForeignLeader: WaitForeignLeader(); break;
			case EGolemArenaNetworkPhase::StaleFocus: WaitStaleFocus(); break;
			case EGolemArenaNetworkPhase::FriendlyFocus: WaitFriendlyFocus(); break;
			case EGolemArenaNetworkPhase::StartWave: WaitStartWave(); break;
			case EGolemArenaNetworkPhase::SightAcquired: WaitSightAcquired(); break;
			case EGolemArenaNetworkPhase::SightLost: WaitSightLost(); break;
			case EGolemArenaNetworkPhase::SightRestored: WaitSightRestored(); break;
			case EGolemArenaNetworkPhase::Focus: WaitFocus(); break;
			case EGolemArenaNetworkPhase::Attack: WaitAttack(); break;
			case EGolemArenaNetworkPhase::CancelAttack: WaitCancelAttack(); break;
			case EGolemArenaNetworkPhase::Revive: WaitRevive(); break;
			case EGolemArenaNetworkPhase::Rejoin: WaitRejoin(); break;
			case EGolemArenaNetworkPhase::OwnerRevive: WaitOwnerRevive(); break;
			case EGolemArenaNetworkPhase::CompleteRevive: WaitCompleteRevive(); break;
			case EGolemArenaNetworkPhase::DestroyTarget: WaitDestroyedTarget(); break;
			case EGolemArenaNetworkPhase::Cleanup: Cleanup(); break;
			default: break;
			}
			return Phase == EGolemArenaNetworkPhase::Complete;
		}

	private:
		void Enter(EGolemArenaNetworkPhase InPhase)
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

		AGolemArenaPlayerController* FindOwner(UWorld* World) const
		{
			if (World == nullptr || World->GetNetMode() != NM_Client)
			{
				return nullptr;
			}
			AGolemPartyState* Party = FindParty(World);
			for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AGolemArenaPlayerController* Player = Cast<AGolemArenaPlayerController>(Iterator->Get());
				AGolemCharacter* Pawn = Player != nullptr ? Player->GetControlledGolem() : nullptr;
				AGolemPlayerState* Human = Player != nullptr ? Player->GetPlayerState<AGolemPlayerState>() : nullptr;
				const FGolemRosterEntry* Entry = Party != nullptr && Human != nullptr
					? Party->FindSlot(Human->GetAssignedSlot()) : nullptr;
				if (Player != nullptr && Player->IsLocalController() && !Player->HasAuthority()
					&& Pawn != nullptr && Pawn->IsLocallyControlled() && Pawn->GetLocalRole() == ROLE_AutonomousProxy
					&& Entry != nullptr && Entry->Golem == Pawn && Entry->HumanPlayerState == Human
					&& Entry->GolemId == Pawn->GetGolemEntityId() && Pawn->GetCombatComponent()->GetProfile() != nullptr
					&& !GetSpecs(Pawn->GetGolemAbilitySystem()).IsEmpty())
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
			Enter(EGolemArenaNetworkPhase::Ready);
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
			AGolemPartyState* Party = FindParty(Server.Get());
			AGolemPartyArenaGameMode* Mode = Server.IsValid() ? Server->GetAuthGameMode<AGolemPartyArenaGameMode>() : nullptr;
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
				AGolemCharacter* Golem = Party->FindSlot(Slot)->Golem;
				Golems.Add(Golem);
				Ids.Add(Golem->GetGolemEntityId());
				Heroes.Add(Golem->GetHeroDefinitionId());
				Specs.Add(GetSpecs(Golem->GetGolemAbilitySystem()));
				Grants.Add(Golem->GetGolemAbilitySystem()->GetOwnedGrantCount());
				if (Grants.Last() < 2 || Golem->GetCombatComponent()->GetProfile() == nullptr)
				{
					Fail(TEXT("Arena did not compose actual foundation and combat source grants."));
					return;
				}
			}
			DisconnectSlot = FindOwner(ContextWorld(ClientContexts[0]))->GetPlayerState<AGolemPlayerState>()->GetAssignedSlot();
			Requester = FindOwner(ContextWorld(ClientContexts[1]));
			RequesterSlot = Requester->GetPlayerState<AGolemPlayerState>()->GetAssignedSlot();
			for (FConstPlayerControllerIterator Iterator = Server->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				AGolemArenaPlayerController* Candidate = Cast<AGolemArenaPlayerController>(Iterator->Get());
				if (Candidate != nullptr && Candidate->IsLocalController())
				{
					Host = Candidate;
					HostSlot = Candidate->GetPlayerState<AGolemPlayerState>()->GetAssignedSlot();
				}
			}
			if (!Host.IsValid())
			{
				Fail(TEXT("Actual listen-server host controller is unavailable."));
				return;
			}
			const TWeakPtr<FGolemArenaNetworkCapture> WeakCapture(Capture);
			ReplyHandle = Requester->OnCompanionCommandResult.AddLambda([WeakCapture](const FGolemResult& Result)
				{
					if (TSharedPtr<FGolemArenaNetworkCapture> Current = WeakCapture.Pin())
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
			ReconnectURL = FString::Printf(TEXT("127.0.0.1:%d?GolemSlot=%d"), Address->GetPort(), DisconnectSlot);
			AGolemArenaPlayerController* Departing = FindOwner(ContextWorld(ClientContexts[0]));
			FScopedConditionalWorldSwitcher Switch(Departing->GetWorld());
			Departing->ClientTravel(DetachedMap, TRAVEL_Absolute);
			Enter(EGolemArenaNetworkPhase::Disconnect);
		}

		bool HasCanonicalIdentities() const
		{
			AGolemPartyState* Party = FindParty(Server.Get());
			if (Party == nullptr || Golems.Num() != 3)
			{
				return false;
			}
			int32 Count = 0;
			for (TActorIterator<AGolemCharacter> Iterator(Server.Get()); Iterator; ++Iterator)
			{
				++Count;
			}
			if (Count != 3)
			{
				return false;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const FGolemRosterEntry* Entry = Party->FindSlot(Slot);
				if (!Golems[Slot].IsValid() || Entry == nullptr || Entry->Golem != Golems[Slot].Get()
					|| Entry->GolemId != Ids[Slot] || Entry->HeroDefinitionId != Heroes[Slot]
					|| Golems[Slot]->GetGolemAbilitySystem()->GetOwnerActor() != Golems[Slot].Get()
					|| Golems[Slot]->GetGolemAbilitySystem()->GetAvatarActor() != Golems[Slot].Get()
					|| GetSpecs(Golems[Slot]->GetGolemAbilitySystem()) != Specs[Slot]
					|| Golems[Slot]->GetGolemAbilitySystem()->GetOwnedGrantCount() != Grants[Slot])
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

		bool SendCommand(EGolemCompanionCommand Command, const FGolemEntityId& ExpectedId,
			const FGolemEntityId& FocusId = FGolemEntityId())
		{
			UFunction* Function = Requester->FindFunction(TEXT("ServerRequestCompanionCommand"));
			struct FGolemCommandParams
			{
				EGolemCompanionCommand Command;
				FGolemEntityId ExpectedGolemId;
				FGolemEntityId FocusId;
			};
			if (Function == nullptr || !Function->HasAnyFunctionFlags(FUNC_NetServer)
				|| Function->ParmsSize != sizeof(FGolemCommandParams))
			{
				Fail(TEXT("Native command RPC reflection contract changed; no direct implementation bypass is allowed."));
				return false;
			}
			PreviousReplies = Capture->ReplyCount;
			FGolemCommandParams Parameters{Command, ExpectedId, FocusId};
			FScopedConditionalWorldSwitcher Switch(Requester->GetWorld());
			Requester->ProcessEvent(Function, &Parameters);
			return true;
		}

		void WaitDisconnect()
		{
			UWorld* Detached = ContextWorld(ClientContexts[0]);
			const FGolemRosterEntry* Entry = FindParty(Server.Get())->FindSlot(DisconnectSlot);
			Companion = Cast<AGolemArenaCompanionController>(Golems[DisconnectSlot]->GetController());
			if (Detached == nullptr || Detached->GetNetMode() != NM_Standalone || Detached->GetNetDriver() != nullptr
				|| Detached->GetFirstPlayerController() == nullptr || Entry->OwnerKind != EGolemControllerOwner::Companion
				|| !Companion.IsValid() || !Companion->GetArenaInitializationResult().IsSuccess()
				|| !Companion->IsFollowing() || !Companion->GetDecisionScheduler()->HasDecisionTimer())
			{
				return;
			}
			Place(Golems[DisconnectSlot].Get(), FVector(-1000.0, -500.0, 100.0));
			Place(Golems[RequesterSlot].Get(), FVector(-900.0, -700.0, 100.0));
			Place(Golems[HostSlot].Get(), FVector(900.0, -900.0, 100.0));
			SourceThreat = Golems[DisconnectSlot]->FindComponentByClass<UGolemThreatComponent>();
			if (!SourceThreat.IsValid() || !SourceThreat->TrySetThreatTuning(0.0f, 1.0f).IsSuccess())
			{
				Fail(TEXT("Ownership fixture requires the native empty threat table with decay disabled to isolate takeover."));
				return;
			}
			const TWeakPtr<FGolemArenaNetworkCapture> WeakCapture(Capture);
			ObservedContext = Companion->GetAIContext();
			ObservedScheduler = Companion->GetDecisionScheduler();
			ActionHandle = ObservedContext->OnActionFinished.AddLambda(
				[WeakCapture](FGuid Token, EGolemAIActionStatus Status)
				{
					if (TSharedPtr<FGolemArenaNetworkCapture> Current = WeakCapture.Pin())
					{
						Current->FinishedActions.Add(Token, Status);
					}
				});
			ScheduleHandle = ObservedScheduler->OnDecisionScheduled.AddLambda(
				[WeakCapture](EGolemAILODTier Tier, float Interval, int64 TickCount)
				{
					if (TSharedPtr<FGolemArenaNetworkCapture> Current = WeakCapture.Pin())
					{
						Current->DecisionTickCount = TickCount;
					}
				});
			Director = Server->GetSubsystem<UGolemCombatDirectorSubsystem>();
			LeaseHandle = Director->OnAttackLeaseReleased.AddLambda(
				[WeakCapture](const FGolemAttackLease& Lease, EGolemAttackLeaseReleaseReason Reason)
				{
					if (TSharedPtr<FGolemArenaNetworkCapture> Current = WeakCapture.Pin())
					{
						Current->ReleasedLeases.Add(Lease.LeaseId, Reason);
					}
				});
			Test.AddInfo(TEXT("Real disconnect preserved the same Golem and started its native arena StateTree/scheduler."));
			if (SendCommand(EGolemCompanionCommand::Hold, Ids[RequesterSlot]))
			{
				Enter(EGolemArenaNetworkPhase::Hold);
			}
		}

		void WaitHold()
		{
			if (Capture->ReplyCount <= PreviousReplies)
			{
				return;
			}
			if (!Capture->Reply.IsSuccess() || Companion->GetCommandSnapshot().Command != EGolemCompanionCommand::Hold
				|| !Companion->GetAIContext()->GetDecisionSnapshot().bHoldPosition)
			{
				Fail(GetDiagnostics(TEXT("Real owned command did not enter Hold")));
				return;
			}
			CommandRevision = Companion->GetCommandSnapshot().CommandRevision;
			if (SendCommand(EGolemCompanionCommand::Regroup, Ids[HostSlot]))
			{
				Enter(EGolemArenaNetworkPhase::StaleGolem);
			}
		}

		bool HasRejectedCommand(EGolemError ExpectedError)
		{
			if (Capture->ReplyCount <= PreviousReplies)
			{
				return false;
			}
			if (Capture->Reply.Error != ExpectedError || Companion->GetCommandSnapshot().CommandRevision != CommandRevision
				|| Companion->GetCommandSnapshot().Command != EGolemCompanionCommand::Hold)
			{
				Fail(GetDiagnostics(TEXT("Rejected RPC mutated command state or returned the wrong typed failure")));
				return false;
			}
			return true;
		}

		void WaitStaleGolem()
		{
			if (HasRejectedCommand(EGolemError::NotOwned))
			{
				Place(Golems[DisconnectSlot].Get(), FVector(1000.0, -900.0, 100.0));
				if (SendCommand(EGolemCompanionCommand::Regroup, Ids[RequesterSlot]))
				{
					Enter(EGolemArenaNetworkPhase::ForeignLeader);
				}
			}
		}

		void WaitForeignLeader()
		{
			if (HasRejectedCommand(EGolemError::NotFound))
			{
				Place(Golems[DisconnectSlot].Get(), FVector(-1000.0, -500.0, 100.0));
				if (SendCommand(EGolemCompanionCommand::Focus, Ids[RequesterSlot], FGolemEntityId::Create()))
				{
					Enter(EGolemArenaNetworkPhase::StaleFocus);
				}
			}
		}

		void WaitStaleFocus()
		{
			if (HasRejectedCommand(EGolemError::InvalidId)
				&& SendCommand(EGolemCompanionCommand::Focus, Ids[RequesterSlot], Ids[HostSlot]))
			{
				Enter(EGolemArenaNetworkPhase::FriendlyFocus);
			}
		}

		void WaitFriendlyFocus()
		{
			if (!HasRejectedCommand(EGolemError::InvalidId))
			{
				return;
			}
			AGolemPartyArenaStartMarker* Marker = nullptr;
			for (TActorIterator<AGolemPartyArenaStartMarker> Iterator(Server.Get()); Iterator; ++Iterator)
			{
				Marker = *Iterator;
				break;
			}
			if (Marker == nullptr)
			{
				Fail(TEXT("Actual M4 start interaction marker is missing."));
				return;
			}
			Test.AddInfo(TEXT("Real RPC rejects stale Golem, another human's follower, stale focus ID and friendly ID without mutation."));
			Place(Golems[HostSlot].Get(), Marker->GetActorLocation() + FVector(0.0, -150.0, 100.0));
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			Host->RequestInteract();
			Enter(EGolemArenaNetworkPhase::StartWave);
		}

		void WaitStartWave()
		{
			AGolemPartyArenaGameMode* Mode = Server->GetAuthGameMode<AGolemPartyArenaGameMode>();
			if (Mode->GetArenaState()->GetSnapshot().Stage != EGolemArenaStage::Wave || Mode->GetEncounterEnemies().IsEmpty())
			{
				return;
			}
			for (AGolemArenaEnemy* Enemy : Mode->GetEncounterEnemies())
			{
				AGolemEnemyController* Controller = Cast<AGolemEnemyController>(Enemy->GetController());
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
			Place(Golems[DisconnectSlot].Get(), FVector(-1080.0, 0.0, 100.0));
			Place(Golems[RequesterSlot].Get(), FVector(-1080.0, -350.0, 100.0));
			Place(Golems[HostSlot].Get(), FVector(1200.0, -1200.0, 100.0));
			Enter(EGolemArenaNetworkPhase::SightAcquired);
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
			Enter(EGolemArenaNetworkPhase::SightLost);
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
			Enter(EGolemArenaNetworkPhase::SightRestored);
		}

		void WaitSightRestored()
		{
			if (!IsTargetActuallySeen())
			{
				return;
			}
			Enter(EGolemArenaNetworkPhase::Focus);
		}

		void WaitFocus()
		{
			// The requesting client picks the nearest enemy it can see from its own replicated copy of the world.
			AActor* ReplicaTarget = bFocusSent ? nullptr
				: Requester->GetControlledGolem()->GetCombatComponent()->FindCombatTarget(2200.0f);
			UGolemAbilitySystemComponent* ReplicaASC = ReplicaTarget != nullptr
				? ReplicaTarget->FindComponentByClass<UGolemAbilitySystemComponent>() : nullptr;
			if (!bFocusSent)
			{
				if (ReplicaASC == nullptr || ReplicaASC->GetEntityId() != Target->GetArenaEnemyId())
				{
					return;
				}
				PreviousReplies = Capture->ReplyCount;
				FScopedConditionalWorldSwitcher Switch(Requester->GetWorld());
				Requester->RequestCompanionCommand(EGolemCompanionCommand::Focus);
				bFocusSent = true;
				return;
			}
			if (Capture->ReplyCount <= PreviousReplies)
			{
				return;
			}
			if (!Capture->Reply.IsSuccess() || Companion->GetCommandSnapshot().Command != EGolemCompanionCommand::Focus
				|| Companion->GetCommandSnapshot().FocusTargetId != Target->GetArenaEnemyId())
			{
				Fail(GetDiagnostics(TEXT("Real nearest-enemy Focus RPC failed")));
				return;
			}
			Enter(EGolemArenaNetworkPhase::Attack);
		}

		void WaitAttack()
		{
			const FGolemAIDecisionSnapshot Snapshot = Companion->GetAIContext()->GetDecisionSnapshot();
			const FGolemAttackLease Lease = Companion->GetAIContext()->GetActiveAttackLease();
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
			if (SendCommand(EGolemCompanionCommand::Hold, Ids[RequesterSlot]))
			{
				Enter(EGolemArenaNetworkPhase::CancelAttack);
			}
		}

		void WaitCancelAttack()
		{
			if (Capture->ReplyCount <= PreviousReplies)
			{
				return;
			}
			const EGolemAIActionStatus* Status = Capture->FinishedActions.Find(AttackToken);
			if (!Capture->Reply.IsSuccess() || Status == nullptr || *Status != EGolemAIActionStatus::Cancelled
				|| Director->IsLeaseActive(AttackLease) || !Capture->ReleasedLeases.Contains(AttackLease.LeaseId))
			{
				Fail(GetDiagnostics(TEXT("Hold did not cancel the captured real GAS action and release its exact lease")));
				return;
			}
			Test.AddInfo(TEXT("Actual owning-client Hold canceled the captured StateTree action and exact director capability."));
			Place(Golems[RequesterSlot].Get(), Golems[DisconnectSlot]->GetActorLocation() + FVector(0.0, -160.0, 0.0));
			FScopedConditionalWorldSwitcher Switch(Server.Get());
			FGolemResult Result = Golems[RequesterSlot]->GetLifeComponent()->TrySetLifeTuning(60.0f, 8.0f, 300.0f, 0.35f);
			if (Result.IsSuccess())
			{
				Result = UGolemAbilitySystemComponent::ApplyDamage(Target.Get(), Golems[DisconnectSlot].Get(),
					10.0f);
			}
			if (Result.IsSuccess())
			{
				Result = UGolemAbilitySystemComponent::ApplyDamage(Target.Get(), Golems[RequesterSlot].Get(),
					Golems[RequesterSlot]->GetGolemAttributeSet()->GetMaxHealth() + 1.0f);
			}
			if (!Result.IsSuccess() || !Golems[RequesterSlot]->GetLifeComponent()->IsDowned())
			{
				Fail(TEXT("Validated GAS setup did not create the real friendly downed revive target."));
				return;
			}
			PersistentHealth = Golems[DisconnectSlot]->GetGolemAttributeSet()->GetHealth();
			PersistentThreat = SourceThreat->GetThreat(Target->GetArenaEnemyId());
			if (PersistentThreat <= 0.0f)
			{
				Fail(TEXT("Actual sight and confirmed GAS damage did not create persistent pawn threat."));
				return;
			}
			Enter(EGolemArenaNetworkPhase::Revive);
		}

		void WaitRevive()
		{
			UGolemAbilitySystemComponent* ASC = Golems[DisconnectSlot]->GetGolemAbilitySystem();
			UGolemLifeComponent* TargetLife = Golems[RequesterSlot]->GetLifeComponent();
			const FGolemAIDecisionSnapshot Snapshot = Companion->GetAIContext()->GetDecisionSnapshot();
			if (!TargetLife->IsReviveInProgress() || TargetLife->GetActiveReviver() != Golems[DisconnectSlot].Get()
				|| !Snapshot.bTargetFriendlyDowned || !Snapshot.bActionRunning
				|| !ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving)
				|| !ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive))
			{
				return;
			}
			ReviveToken = Snapshot.ActiveActionToken;
			Test.AddInfo(FString::Printf(TEXT("Real companion shared-GAS revive active token=%s; same-slot JIP requested midchannel."),
				*ReviveToken.ToString()));
			UWorld* Detached = ContextWorld(ClientContexts[0]);
			FScopedConditionalWorldSwitcher Switch(Detached);
			Detached->GetFirstPlayerController()->ClientTravel(ReconnectURL, TRAVEL_Absolute);
			Enter(EGolemArenaNetworkPhase::Rejoin);
		}

		bool HasClientSnapshots() const
		{
			const AGolemPartyState* Authority = FindParty(Server.Get());
			for (FName Handle : ClientContexts)
			{
				UWorld* World = ContextWorld(Handle);
				const AGolemPartyState* Replica = FindParty(World);
				if (World == nullptr || World->GetNetMode() != NM_Client || Replica == nullptr
					|| !Replica->IsCanonicalRosterReady() || Replica->GetRosterRevision() != Authority->GetRosterRevision())
				{
					return false;
				}
				int32 Count = 0;
				for (TActorIterator<AGolemCharacter> Iterator(World); Iterator; ++Iterator)
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
					const FGolemRosterEntry* Entry = Replica->FindSlot(Slot);
					UGolemAbilitySystemComponent* ASC = Entry != nullptr && Entry->Golem != nullptr
						? Entry->Golem->GetGolemAbilitySystem() : nullptr;
					if (ASC == nullptr || Entry->GolemId != Ids[Slot] || Entry->HeroDefinitionId != Heroes[Slot]
						|| ASC->GetOwnerActor() != Entry->Golem || ASC->GetAvatarActor() != Entry->Golem
						|| Entry->Golem->GetLifeComponent()->GetLifeState() != Golems[Slot]->GetLifeComponent()->GetLifeState()
						|| ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive)
							!= Golems[Slot]->GetGolemAbilitySystem()->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive))
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
			if (!Reclaimed.IsValid() || Reclaimed->GetPlayerState<AGolemPlayerState>()->GetAssignedSlot() != DisconnectSlot
				|| FindParty(Server.Get())->GetHumanCount() != 3 || !HasClientSnapshots())
			{
				return;
			}
			const EGolemAIActionStatus* Status = Capture->FinishedActions.Find(ReviveToken);
			UGolemAbilitySystemComponent* ASC = Golems[DisconnectSlot]->GetGolemAbilitySystem();
			UGolemLifeComponent* TargetLife = Golems[RequesterSlot]->GetLifeComponent();
			const bool bControllerClean = !Companion.IsValid() || (Companion->GetPawn() == nullptr && !Companion->IsFollowing());
			const bool bContextClean = !ObservedContext.IsValid() || (ObservedContext->GetControlledPawn() == nullptr
				&& !ObservedContext->GetActiveAttackLease().IsValid() && ObservedContext->GetActiveEQSRequestIds().IsEmpty());
			const bool bSchedulerClean = !ObservedScheduler.IsValid() || !ObservedScheduler->HasDecisionTimer();
			if (Status == nullptr || *Status != EGolemAIActionStatus::Cancelled || !TargetLife->IsDowned()
				|| TargetLife->IsReviveInProgress() || ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving)
				|| ASC->IsGameplayCueActive(GolemGameplayTags::GameplayCue_Revive) || !HasInactiveInputs(ASC)
				|| !bControllerClean || !bContextClean || !bSchedulerClean || Director->GetActiveLeaseCount() != 0
				|| GetSpecs(Reclaimed->GetControlledGolem()->GetGolemAbilitySystem()) != Specs[DisconnectSlot]
				|| !FMath::IsNearlyEqual(Golems[DisconnectSlot]->GetGolemAttributeSet()->GetHealth(), PersistentHealth)
				|| SourceThreat->GetThreat(Target->GetArenaEnemyId()) < PersistentThreat)
			{
				Fail(GetDiagnostics(TEXT("Same-slot JIP did not clean exact AI action/reservation/cue/tree/timer or preserve persistent state")));
				return;
			}
			AGolemPartyArenaGameMode* Mode = Server->GetAuthGameMode<AGolemPartyArenaGameMode>();
			for (FName Handle : ClientContexts)
			{
				UWorld* ClientWorld = ContextWorld(Handle);
				if (ClientWorld == nullptr || ClientWorld->GetNetMode() != NM_Client)
				{
					return;
				}
				for (AGolemArenaEnemy* Enemy : Mode->GetEncounterEnemies())
				{
					AGolemArenaEnemy* Replica = nullptr;
					for (TActorIterator<AGolemArenaEnemy> Iterator(ClientWorld); Iterator; ++Iterator)
					{
						if (Iterator->GetArenaEnemyId() == Enemy->GetArenaEnemyId())
						{
							Replica = *Iterator;
							break;
						}
					}
					if (Replica == nullptr || Replica->GetGolemAbilitySystem()->GetEntityId() != Enemy->GetArenaEnemyId()
						|| Replica->GetCombatComponent()->GetProfile() == nullptr)
					{
						return;
					}
					const UTextRenderComponent* Caption = Replica->FindComponentByClass<UTextRenderComponent>();
					if (Caption == nullptr || !Caption->Text.EqualTo(NSLOCTEXT("GolemArena", "EnemyLabel", "ARENA ENEMY")))
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
				Reclaimed->RequestAbilityInput(GolemGameplayTags::Input_Party_Revive, true);
			}
			Enter(EGolemArenaNetworkPhase::OwnerRevive);
		}

		void WaitOwnerRevive()
		{
			UGolemLifeComponent* Life = Golems[RequesterSlot]->GetLifeComponent();
			if (!Life->IsReviveInProgress() || Life->GetActiveReviver() != Golems[DisconnectSlot].Get()
				|| !Golems[DisconnectSlot]->GetGolemAbilitySystem()->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving))
			{
				return;
			}
			bObservedOwnerRevive = true;
			Enter(EGolemArenaNetworkPhase::CompleteRevive);
		}

		void WaitCompleteRevive()
		{
			if (!Golems[RequesterSlot]->GetLifeComponent()->IsAlive())
			{
				return;
			}
			if (!bReleasedOwnerInput)
			{
				FScopedConditionalWorldSwitcher Switch(Reclaimed->GetWorld());
				Reclaimed->RequestAbilityInput(GolemGameplayTags::Input_Party_Revive, false);
				bReleasedOwnerInput = true;
				return;
			}
			if (!HasClientSnapshots() || !HasInactiveInputs(Golems[DisconnectSlot]->GetGolemAbilitySystem())
				|| !HasInactiveInputs(Reclaimed->GetControlledGolem()->GetGolemAbilitySystem())
				|| Golems[RequesterSlot]->GetLifeComponent()->IsReviveInProgress())
			{
				return;
			}
			if (!bObservedOwnerRevive || Golems[RequesterSlot]->GetGolemAttributeSet()->GetHealth() <= 0.0f
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
			Enter(EGolemArenaNetworkPhase::DestroyTarget);
		}

		void WaitDestroyedTarget()
		{
			if (SourceThreat->GetThreat(DestroyedTargetId) > 0.0f
				|| SourceThreat->GetTrackedEntryCount() != ThreatEntriesBeforeDestroy - 1)
			{
				return;
			}
			const FGolemPartyArenaSnapshot& ServerSnapshot = Server->GetAuthGameMode<AGolemPartyArenaGameMode>()
				->GetArenaState()->GetSnapshot();
			if (ServerSnapshot.Stage != EGolemArenaStage::Defeat
				|| ServerSnapshot.Failure != EGolemArenaFailure::EncounterUnavailable)
			{
				return;
			}
			for (FName Handle : ClientContexts)
			{
				AGolemPartyArenaState* Replica = nullptr;
				for (TActorIterator<AGolemPartyArenaState> Iterator(ContextWorld(Handle)); Iterator; ++Iterator)
				{
					Replica = *Iterator;
					break;
				}
				if (Replica == nullptr || Replica->GetSnapshot().Revision != ServerSnapshot.Revision
					|| Replica->GetSnapshot().Stage != EGolemArenaStage::Defeat
					|| Replica->GetSnapshot().Failure != EGolemArenaFailure::EncounterUnavailable)
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
			const FGolemAIDecisionSnapshot Snapshot = Companion.IsValid()
				? Companion->GetAIContext()->GetDecisionSnapshot() : FGolemAIDecisionSnapshot();
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
				Golems.IsValidIndex(RequesterSlot) && Golems[RequesterSlot].IsValid()
					&& Golems[RequesterSlot]->GetLifeComponent()->IsDowned(),
				Golems.IsValidIndex(RequesterSlot) && Golems[RequesterSlot].IsValid()
					&& Golems[RequesterSlot]->GetLifeComponent()->IsReviveInProgress(),
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
			if (Phase >= EGolemArenaNetworkPhase::Cleanup)
			{
				return;
			}
			UnbindObservers();
			Enter(EGolemArenaNetworkPhase::Cleanup);
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
			Enter(EGolemArenaNetworkPhase::Complete);
		}

		FAutomationTestBase& Test;
		TSharedPtr<FGolemArenaNetworkCapture> Capture;
		EGolemArenaNetworkPhase Phase = EGolemArenaNetworkPhase::Start;
		double PhaseStarted = 0.0;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize;
		FIntPoint OriginalWindowPosition;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		TWeakObjectPtr<UWorld> Server;
		TArray<FName> ClientContexts;
		TWeakObjectPtr<AGolemArenaPlayerController> Host;
		TWeakObjectPtr<AGolemArenaPlayerController> Requester;
		TWeakObjectPtr<AGolemArenaPlayerController> Reclaimed;
		TWeakObjectPtr<AGolemArenaCompanionController> Companion;
		TWeakObjectPtr<UGolemCombatDirectorSubsystem> Director;
		TWeakObjectPtr<UGolemAIContextComponent> ObservedContext;
		TWeakObjectPtr<UGolemAIDecisionSchedulerComponent> ObservedScheduler;
		TWeakObjectPtr<UGolemThreatComponent> SourceThreat;
		TWeakObjectPtr<AGolemArenaEnemy> Target;
		TArray<TWeakObjectPtr<AGolemCharacter>> Golems;
		TArray<FGolemEntityId> Ids;
		TArray<FPrimaryAssetId> Heroes;
		TArray<TArray<FGameplayAbilitySpecHandle>> Specs;
		TArray<int32> Grants;
		FDelegateHandle ReplyHandle;
		FDelegateHandle ActionHandle;
		FDelegateHandle LeaseHandle;
		FDelegateHandle ScheduleHandle;
		FString ReconnectURL;
		FGolemAttackLease AttackLease;
		FGuid AttackToken;
		FGuid ReviveToken;
		FGolemEntityId DestroyedTargetId;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemArenaNetworkIntegrationTest, "Golem.AI.Arena.NetworkCommandsTakeoverJIP",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemArenaNetworkIntegrationTest::RunTest(const FString& Parameters)
{
	FAutomationTestFramework::Get().EnqueueLatentCommand(
		MakeShared<GolemArenaNetwork::FGolemArenaNetworkCommand>(*this));
	return true;
}

#endif
