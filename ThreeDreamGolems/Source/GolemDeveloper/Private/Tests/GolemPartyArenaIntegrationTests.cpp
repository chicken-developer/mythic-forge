#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Abilities/GolemHeavyAttackAbility.h"
#include "Abilities/GolemLightAttackAbility.h"
#include "Abilities/GolemReviveAbility.h"
#include "Attributes/GolemAttributeSet.h"
#include "Boss/GolemBossPhaseComponent.h"
#include "Camera/GolemCameraModeComponent.h"
#include "Character/GolemCharacter.h"
#include "Combat/GolemCombatDirectorSubsystem.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemAIDecisionSchedulerComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Context/GolemAIContextComponent.h"
#include "Controllers/GolemArenaCompanionController.h"
#include "Controllers/GolemEnemyController.h"
#include "Definitions/GolemAIActionSetDefinition.h"
#include "Party/GolemPartyState.h"
#include "PartyArena/GolemArenaEnemy.h"
#include "PartyArena/GolemArenaPlayerController.h"
#include "PartyArena/GolemPartyArenaGameMode.h"
#include "PartyArena/GolemPartyArenaHUD.h"
#include "PartyArena/GolemPartyArenaState.h"
#include "Perception/GolemAIRolePerceptionComponent.h"
#include "Tags/GolemGameplayTags.h"

#include "Animation/AnimInstance.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "FileHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayAbilitySpec.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/PackageName.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemArenaPlaythrough, Log, All);

namespace GolemPartyArenaTest
{
	const TCHAR* MapPath = TEXT("/Game/Golem/Maps/Playables/L_M04_PartyArena");
	constexpr double BossHeavyObservationTimeout = 15.0;

	enum class EGolemArenaTestPhase : uint8
	{
		Navigation,
		Start,
		Ready,
		FollowObstacle,
		Hold,
		Regroup,
		StartEncounter,
		QueryPositive,
		QueryUnreachable,
		QueryCancel,
		FocusCommand,
		CompanionRevive,
		Wave,
		Boss,
		VictoryRetry,
		WipeStart,
		Wipe,
		DefeatRetry,
		Cleanup,
		Complete
	};

	struct FGolemArenaQueryObservation
	{
		bool bAcceptCallbacks = true;
		bool bCompleted = false;
		bool bSucceeded = false;
		bool bAborted = false;
		int32 CallbackCount = 0;
		int32 ResultCount = 0;
		FVector Location = FVector::ZeroVector;
	};

	class FGolemArenaScenario final : public IAutomationLatentCommand
	{
	public:
		explicit FGolemArenaScenario(FAutomationTestBase& InTest)
			: Test(InTest), RunStarted(FPlatformTime::Seconds()), PhaseStarted(RunStarted)
		{
		}

		virtual ~FGolemArenaScenario() override
		{
			AbortOwnedQuery();
			ReleaseInput();
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
				RestoreSettings();
			}
		}

		virtual bool Update() override
		{
			if (Phase == EGolemArenaTestPhase::Complete)
			{
				return true;
			}
			if (Phase != EGolemArenaTestPhase::Cleanup
				&& (FPlatformTime::Seconds() - RunStarted > 300.0
					|| FPlatformTime::Seconds() - PhaseStarted > PhaseTimeout))
			{
				Fail(TEXT("Bounded real arena playthrough phase timed out."));
			}
			if (Phase != EGolemArenaTestPhase::Cleanup && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			if (World.IsValid() && Phase > EGolemArenaTestPhase::Ready && Phase < EGolemArenaTestPhase::Cleanup)
			{
				if (!ValidateCanonicalParty())
				{
					BeginCleanup();
				}
				else
				{
					ObserveCombat();
					if (PulseKey.IsValid() && World->GetTimeSeconds() - PulseStarted > 0.08)
					{
						SetKey(PulseKey, false);
						PulseKey = FKey();
					}
				}
			}
			switch (Phase)
			{
			case EGolemArenaTestPhase::Navigation:
				if (NavigationWait.Update())
				{
					Enter(EGolemArenaTestPhase::Start, 30.0);
				}
				break;
			case EGolemArenaTestPhase::Start:
				Start();
				break;
			case EGolemArenaTestPhase::Ready:
				Ready();
				break;
			case EGolemArenaTestPhase::FollowObstacle:
				FollowObstacle();
				break;
			case EGolemArenaTestPhase::Hold:
				if (WorldElapsed() > 0.4 && HasCommand(EGolemCompanionCommand::Hold))
				{
					Pulse(EKeys::K);
					Enter(EGolemArenaTestPhase::Regroup);
				}
				break;
			case EGolemArenaTestPhase::Regroup:
				if (WorldElapsed() > 0.4 && HasCommand(EGolemCompanionCommand::Regroup))
				{
					PlaceControlledGolem(FVector(0.0, -820.0, 100.0), FRotator(0.0, 90.0, 0.0));
					Pulse(EKeys::F);
					Enter(EGolemArenaTestPhase::StartEncounter);
				}
				break;
			case EGolemArenaTestPhase::StartEncounter:
				if (State->GetSnapshot().Stage == EGolemArenaStage::Wave && Mode->GetEncounterEnemies().Num() == 2)
				{
					BeginQueryChecks();
				}
				break;
			case EGolemArenaTestPhase::QueryPositive:
				CheckPositiveQuery();
				break;
			case EGolemArenaTestPhase::QueryUnreachable:
				CheckUnreachableQuery();
				break;
			case EGolemArenaTestPhase::QueryCancel:
				CheckCancelledQuery();
				break;
			case EGolemArenaTestPhase::FocusCommand:
				if (WorldElapsed() > 0.4 && HasCommand(EGolemCompanionCommand::Focus))
				{
					for (int32 Slot = 1; Slot < 3; ++Slot)
					{
						AGolemArenaCompanionController* AI = CastChecked<AGolemArenaCompanionController>(
							Golems[Slot]->GetController());
						if (AI->GetCommandSnapshot().FocusTargetId != QueryEnemy->GetArenaEnemyId())
						{
							Fail(TEXT("Mapped Focus command did not preserve the exact validated hostile target identity."));
							return false;
						}
					}
					BeginCompanionRevive();
				}
				break;
			case EGolemArenaTestPhase::CompanionRevive:
				CheckCompanionRevive();
				break;
			case EGolemArenaTestPhase::Wave:
				FightWave();
				break;
			case EGolemArenaTestPhase::Boss:
				FightBoss();
				break;
			case EGolemArenaTestPhase::VictoryRetry:
				if (WorldElapsed() > 0.6 && ValidateReset())
				{
					PlaceControlledGolem(FVector(0.0, -820.0, 100.0), FRotator(0.0, 90.0, 0.0));
					Pulse(EKeys::F);
					Enter(EGolemArenaTestPhase::WipeStart);
				}
				break;
			case EGolemArenaTestPhase::WipeStart:
				if (State->GetSnapshot().Stage == EGolemArenaStage::Wave && Mode->GetEncounterEnemies().Num() == 2)
				{
					InjectPartyWipe();
				}
				break;
			case EGolemArenaTestPhase::Wipe:
				if (WorldElapsed() > 0.3 && State->GetSnapshot().Stage == EGolemArenaStage::Defeat)
				{
					if (!HUD->IsDisplayingArenaDefeat() || Director->GetActiveLeaseCount() != 0)
					{
						Fail(TEXT("Real wipe did not display Defeat and release active attack leases."));
						break;
					}
					Pulse(EKeys::R);
					Enter(EGolemArenaTestPhase::DefeatRetry);
				}
				break;
			case EGolemArenaTestPhase::DefeatRetry:
				if (WorldElapsed() > 0.6 && ValidateReset())
				{
					Test.AddInfo(TEXT("Actual M4 mapped victory and wipe both reset stable actors, grants, tokens, life and input."));
					BeginCleanup();
				}
				break;
			case EGolemArenaTestPhase::Cleanup:
				Cleanup();
				break;
			default:
				break;
			}
			return Phase == EGolemArenaTestPhase::Complete;
		}

	private:
		void Enter(const EGolemArenaTestPhase Next, const double Timeout = 15.0)
		{
			Phase = Next;
			PhaseStarted = FPlatformTime::Seconds();
			PhaseWorldStarted = World.IsValid() ? World->GetTimeSeconds() : 0.0;
			PhaseTimeout = Timeout;
			Test.AddInfo(FString::Printf(TEXT("Arena phase %d started."), static_cast<int32>(Phase)));
			UE_LOG(LogGolemArenaPlaythrough, Display, TEXT("Actual arena phase%d started."), static_cast<int32>(Phase));
		}

		double WorldElapsed() const
		{
			return World.IsValid() ? World->GetTimeSeconds() - PhaseWorldStarted : 0.0;
		}

		void SetKey(const FKey& Key, const bool bPressed)
		{
			if (!Controller.IsValid())
			{
				return;
			}
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key,
				bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f));
			if (bPressed)
			{
				DownKeys.Add(Key);
			}
			else
			{
				DownKeys.Remove(Key);
			}
		}

		void Pulse(const FKey& Key)
		{
			if (PulseKey.IsValid())
			{
				SetKey(PulseKey, false);
			}
			PulseKey = Key;
			PulseStarted = World.IsValid() ? World->GetTimeSeconds() : 0.0;
			SetKey(Key, true);
		}

		void ReleaseInput()
		{
			const TArray<FKey> Keys = DownKeys.Array();
			for (const FKey& Key : Keys)
			{
				SetKey(Key, false);
			}
			PulseKey = FKey();
		}

		void Start()
		{
			if (GEditor == nullptr || GEditor->IsPlaySessionInProgress())
			{
				Fail(TEXT("Arena automation requires an idle Editor."));
				return;
			}
			const ULevelEditorPlaySettings* Defaults = GetDefault<ULevelEditorPlaySettings>();
			OriginalPositions = Defaults->MultipleInstancePositions;
			OriginalLastSize = Defaults->LastSize;
			OriginalPosition = Defaults->NewWindowPosition;
			OriginalWidth = Defaults->NewWindowWidth;
			OriginalHeight = Defaults->NewWindowHeight;
			Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(Defaults, GetTransientPackage()));
			Settings->SetPlayNetMode(PIE_Standalone);
			Settings->SetPlayNumberOfClients(1);
			Settings->SetRunUnderOneProcess(true);
			Settings->bLaunchSeparateServer = false;
			Settings->EnableGameSound = false;
			Settings->SetClientWindowSize(FIntPoint(1280, 960));
			FRequestPlaySessionParams Request;
			Request.EditorPlaySettings = Settings.Get();
			Request.bAllowOnlineSubsystem = false;
			GEditor->RequestPlaySession(Request);
			bOwnsSession = true;
			Enter(EGolemArenaTestPhase::Ready, 35.0);
		}

		void Ready()
		{
			UWorld* PIEWorld = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
			if (PIEWorld == nullptr || !PIEWorld->HasBegunPlay())
			{
				return;
			}
			World = PIEWorld;
			Mode = PIEWorld->GetAuthGameMode<AGolemPartyArenaGameMode>();
			Controller = Cast<AGolemArenaPlayerController>(PIEWorld->GetFirstPlayerController());
			Party = Mode.IsValid() ? Mode->GetPartyState() : nullptr;
			State = Mode.IsValid() ? Mode->GetArenaState() : nullptr;
			HUD = Controller.IsValid() ? Cast<AGolemPartyArenaHUD>(Controller->GetHUD()) : nullptr;
			Director = PIEWorld->GetSubsystem<UGolemCombatDirectorSubsystem>();
			if (!Party.IsValid() || !State.IsValid() || !Controller.IsValid() || !HUD.IsValid() || !Director.IsValid()
				|| !Party->IsCanonicalRosterReady() || Controller->GetControlledGolem() == nullptr)
			{
				return;
			}
			if (!Mode->GetArenaInitializationResult().IsSuccess())
			{
				Fail(Mode->GetArenaInitializationResult().Message.ToString());
				return;
			}
			if (ReadyWorldTime < 0.0)
			{
				ReadyWorldTime = World->GetTimeSeconds();
			}
			if (World->GetTimeSeconds() - ReadyWorldTime < 0.5)
			{
				return;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const FGolemRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || Entry->Golem == nullptr)
				{
					Fail(TEXT("Canonical arena roster was not fully initialized."));
					return;
				}
				Golems.Add(Entry->Golem.Get());
				GolemIds.Add(Entry->GolemId);
				HeroIds.Add(Entry->HeroDefinitionId);
			}
			if (!ValidateCanonicalParty() || State->GetSnapshot().Stage != EGolemArenaStage::AwaitingStart)
			{
				Fail(TEXT("Arena entry did not expose exactly three configured Golems and AwaitingStart."));
				return;
			}
			for (int32 Slot = 1; Slot < 3; ++Slot)
			{
				AGolemArenaCompanionController* AI = Cast<AGolemArenaCompanionController>(Golems[Slot]->GetController());
				if (AI == nullptr || !AI->GetArenaInitializationResult().IsSuccess()
					|| !AI->GetDecisionScheduler()->HasDecisionTimer())
				{
					Fail(TEXT("Arena companion did not run a real native StateTree decision scheduler."));
					return;
				}
				FollowDistances.Add(FVector::Dist2D(Golems[Slot]->GetActorLocation(), FVector(-300.0, 350.0, 100.0)));
			}
			PlaceControlledGolem(FVector(-300.0, 350.0, 100.0), FRotator(0.0, 90.0, 0.0));
			HumanMovementOrigin = Controller->GetControlledGolem()->GetActorLocation();
			SetKey(EKeys::W, true);
			Enter(EGolemArenaTestPhase::FollowObstacle, 15.0);
		}

		bool ValidateCanonicalParty()
		{
			if (!World.IsValid() || !Party.IsValid() || !Controller.IsValid() || !State.IsValid() || Golems.Num() != 3)
			{
				Fail(TEXT("Arena lost its live canonical party."));
				return false;
			}
			int32 Count = 0;
			for (TActorIterator<AGolemCharacter> Iterator(World.Get()); Iterator; ++Iterator)
			{
				++Count;
			}
			if (Count != 3 || Party->GetHumanCount() != 1)
			{
				Fail(TEXT("Arena created an extra canonical Golem or changed solo human ownership."));
				return false;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const FGolemRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || Entry->Golem != Golems[Slot].Get() || Entry->GolemId != GolemIds[Slot]
					|| Entry->HeroDefinitionId != HeroIds[Slot]
					|| Entry->Golem->GetGolemAbilitySystem()->GetOwnedGrantCount() != 2
					|| Entry->Golem->GetGolemAbilitySystem()->GetActivatableAbilities().Num() != 6)
				{
					Fail(TEXT("Arena replaced identity or duplicated/lost shared foundation/combat grants."));
					return false;
				}
			}
			if (Director->GetActiveLeaseCount() > Director->GetGlobalBudget())
			{
				Fail(TEXT("Real active attack lease count exceeded the configured global budget."));
				return false;
			}
			for (const FGolemEntityId& Id : GolemIds)
			{
				if (Director->GetActiveLeaseCountForTarget(Id) > Director->GetPerTargetBudget())
				{
					Fail(TEXT("Real concurrent attacker count exceeded the per-target budget."));
					return false;
				}
			}
			return true;
		}

		void FollowObstacle()
		{
			if (WorldElapsed() > 0.5 && DownKeys.Contains(EKeys::W))
			{
				SetKey(EKeys::W, false);
			}
			if (WorldElapsed() < 5.0)
			{
				return;
			}
			if (FVector::Dist2D(HumanMovementOrigin, Controller->GetControlledGolem()->GetActorLocation()) < 50.0)
			{
				Fail(TEXT("Mapped W did not move the controlled Golem in the real CharacterMovement world."));
				return;
			}
			for (int32 Slot = 1; Slot < 3; ++Slot)
			{
				const double Remaining = FVector::Dist2D(Golems[Slot]->GetActorLocation(),
					Controller->GetControlledGolem()->GetActorLocation());
				if (Remaining > FollowDistances[Slot - 1] - 400.0)
				{
					return;
				}
			}
			Test.AddInfo(TEXT("Both native companions followed real nav paths across the arena LOS obstacle."));
			Pulse(EKeys::Z);
			Enter(EGolemArenaTestPhase::Hold);
		}

		bool HasCommand(const EGolemCompanionCommand Command)
		{
			if (!Controller->GetLastCompanionCommandResult().IsSuccess()
				|| Controller->GetLastCompanionCommand() != Command || HUD->GetDisplayedCommandFeedback().IsEmpty())
			{
				return false;
			}
			for (int32 Slot = 1; Slot < 3; ++Slot)
			{
				AGolemArenaCompanionController* AI = Cast<AGolemArenaCompanionController>(Golems[Slot]->GetController());
				if (AI == nullptr || AI->GetCommandSnapshot().Command != Command
					|| AI->GetCommandSnapshot().CommandRevision < 1)
				{
					return false;
				}
			}
			return true;
		}

		void PlaceControlledGolem(const FVector& Location, const FRotator& Rotation)
		{
			ReleaseInput();
			AGolemCharacter* Golem = Controller->GetControlledGolem();
			Golem->GetCharacterMovement()->StopMovementImmediately();
			Golem->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
			Controller->SetControlRotation(Rotation);
		}

		void BeginQueryChecks()
		{
			QueryEnemy = Mode->GetEncounterEnemies()[0].Get();
			QueryController = QueryEnemy.IsValid() ? Cast<AGolemEnemyController>(QueryEnemy->GetController()) : nullptr;
			if (!QueryController.IsValid() || !QueryController->GetInitializationResult().IsSuccess()
				|| QueryController->GetDecisionTree() == nullptr || !QueryController->GetDecisionTree()->IsRunning())
			{
				Fail(TEXT("Wave enemy did not initialize a genuine running StateTree."));
				return;
			}
			QueryEnemyTransform = QueryEnemy->GetActorTransform();
			QueryController->GetDecisionTree()->StopLogic(TEXT("Explicit bounded EQS negative-case fixture"));
			QueryController->StopMovement();
			if (!QueryController->GetAIContext()->SetValidatedTarget(Controller->GetControlledGolem()).IsSuccess())
			{
				Fail(TEXT("Real EQS querier rejected the validated canonical target."));
				return;
			}
			QueryTemplate = QueryController->GetAIContext()->GetRepositionQuery();
			if (!StartOwnedQuery())
			{
				return;
			}
			Enter(EGolemArenaTestPhase::QueryPositive);
		}

		bool StartOwnedQuery()
		{
			QueryObservation = MakeShared<FGolemArenaQueryObservation>();
			TWeakPtr<FGolemArenaQueryObservation> WeakObservation = QueryObservation;
			FQueryFinishedSignature Callback = FQueryFinishedSignature::CreateLambda(
				[WeakObservation](TSharedPtr<FEnvQueryResult> Result)
				{
					TSharedPtr<FGolemArenaQueryObservation> Observation = WeakObservation.Pin();
					if (!Observation.IsValid() || !Observation->bAcceptCallbacks || !Result.IsValid())
					{
						return;
					}
					++Observation->CallbackCount;
					Observation->bCompleted = true;
					Observation->bSucceeded = Result->IsSuccessful();
					Observation->bAborted = Result->IsAborted();
					Observation->ResultCount = Result->Items.Num();
					if (Result->IsSuccessful() && !Result->Items.IsEmpty())
					{
						Observation->Location = Result->GetItemAsLocation(0);
					}
				});
			FEnvQueryRequest Request(QueryTemplate.Get(), QueryEnemy.Get());
			Request.SetWorldOverride(World.Get());
			OwnedQueryId = Request.Execute(EEnvQueryRunMode::SingleResult, Callback);
			if (OwnedQueryId == INDEX_NONE)
			{
				Fail(TEXT("Actual EQS manager could not start the saved combat-position query."));
				return false;
			}
			return true;
		}

		void CheckPositiveQuery()
		{
			if (!QueryObservation->bCompleted)
			{
				return;
			}
			OwnedQueryId = INDEX_NONE;
			UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World.Get());
			FNavLocation Projected;
			const FVector Location = QueryObservation->Location;
			AGolemCharacter* Target = Controller->GetControlledGolem();
			FHitResult Hit;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(Target);
			Params.AddIgnoredActor(QueryEnemy.Get());
			const bool bBlocked = World->LineTraceSingleByChannel(Hit, Location + FVector(0.0, 0.0, 70.0),
				Target->GetActorLocation(), ECC_Visibility, Params);
			const double Distance = FVector::Dist2D(Location, Target->GetActorLocation());
			UNavigationPath* Path = Navigation != nullptr ? Navigation->FindPathToLocationSynchronously(
				World.Get(), QueryEnemy->GetActorLocation(), Location, QueryEnemy.Get()) : nullptr;
			if (!QueryObservation->bSucceeded || QueryObservation->CallbackCount != 1
				|| QueryObservation->ResultCount < 1 || Location.ContainsNaN() || Distance < 119.0 || Distance > 501.0
				|| Navigation == nullptr || !Navigation->ProjectPointToNavigation(Location, Projected)
				|| Path == nullptr || !Path->IsValid() || Path->IsPartial() || bBlocked)
			{
				Fail(TEXT("Actual EQS result failed independent range/nav/path/LOS validation."));
				return;
			}
			Test.AddInfo(FString::Printf(TEXT("Real saved EQS returned %s, range %.1f, reachable path and clear LOS."),
				*Location.ToString(), Distance));
			QueryEnemy->GetCharacterMovement()->StopMovementImmediately();
			QueryEnemy->SetActorLocation(FVector(5000.0, 0.0, 100.0), false, nullptr, ETeleportType::TeleportPhysics);
			if (StartOwnedQuery())
			{
				Enter(EGolemArenaTestPhase::QueryUnreachable);
			}
		}

		void CheckUnreachableQuery()
		{
			if (!QueryObservation->bCompleted)
			{
				return;
			}
			OwnedQueryId = INDEX_NONE;
			if (QueryObservation->bSucceeded || QueryObservation->CallbackCount != 1 || QueryObservation->bAborted)
			{
				Fail(TEXT("Unreachable actual querier produced a success result instead of bounded path failure."));
				return;
			}
			QueryEnemy->SetActorTransform(QueryEnemyTransform, false, nullptr, ETeleportType::TeleportPhysics);
			QueryEnemy->GetCharacterMovement()->StopMovementImmediately();
			if (!StartOwnedQuery())
			{
				return;
			}
			UEnvQueryManager* Manager = UEnvQueryManager::GetCurrent(World.Get());
			if (Manager == nullptr || !Manager->AbortQuery(OwnedQueryId))
			{
				Fail(TEXT("Actual asynchronous EQS cancellation failed."));
				return;
			}
			OwnedQueryId = INDEX_NONE;
			Enter(EGolemArenaTestPhase::QueryCancel);
		}

		void CheckCancelledQuery()
		{
			if (WorldElapsed() < 0.25)
			{
				return;
			}
			if (!QueryObservation->bCompleted || !QueryObservation->bAborted || QueryObservation->bSucceeded
				|| QueryObservation->CallbackCount != 1)
			{
				Fail(TEXT("Canceled EQS did not deliver one real aborted completion without a success result."));
				return;
			}
			QueryController->GetDecisionTree()->RestartLogic();
			PlaceControlledGolem(QueryEnemy->GetActorLocation() - FVector(0.0, 300.0, 0.0),
				FRotator(0.0, 90.0, 0.0));
			// Focus takes the nearest enemy in sight, which is the one the Golem was just placed in front of.
			Pulse(EKeys::B);
			Enter(EGolemArenaTestPhase::FocusCommand);
		}

		void BeginCompanionRevive()
		{
			AGolemCharacter* Downed = Golems[1].Get();
			AGolemCharacter* Reviver = Golems[2].Get();
			Downed->SetActorLocation(FVector(-950.0, -450.0, 100.0), false, nullptr, ETeleportType::TeleportPhysics);
			Reviver->SetActorLocation(FVector(-1080.0, -450.0, 100.0), false, nullptr, ETeleportType::TeleportPhysics);
			PlaceControlledGolem(FVector(500.0, -850.0, 100.0), FRotator::ZeroRotator);
			Pulse(EKeys::Z);
			if (!UGolemAbilitySystemComponent::ApplyDamage(QueryEnemy.Get(), Downed, 1000.0f).IsSuccess()
				|| !Downed->GetLifeComponent()->IsDowned())
			{
				Fail(TEXT("Real GAS downed fault did not initialize the companion revive scenario."));
				return;
			}
			Enter(EGolemArenaTestPhase::CompanionRevive, 20.0);
		}

		void CheckCompanionRevive()
		{
			AGolemCharacter* Downed = Golems[1].Get();
			AGolemCharacter* Reviver = Golems[2].Get();
			const FGameplayAbilitySpec* ReviveSpec = Reviver->GetGolemAbilitySystem()->FindAbilitySpecFromClass(
				UGolemReviveAbility::StaticClass());
			if (Downed->GetLifeComponent()->IsReviveInProgress()
				&& Downed->GetLifeComponent()->GetActiveReviver() == Reviver && ReviveSpec != nullptr && ReviveSpec->IsActive()
				&& Reviver->GetGolemAbilitySystem()->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving))
			{
				bObservedCompanionRevive = true;
			}
			if (Downed->GetLifeComponent()->IsAlive())
			{
				if (!bObservedCompanionRevive || Downed->GetLifeComponent()->IsReviveInProgress()
					|| ReviveSpec == nullptr || ReviveSpec->IsActive()
					|| Reviver->GetGolemAbilitySystem()->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving))
				{
					Fail(TEXT("Companion revive did not execute and clean the actual shared GAS ability/reservation."));
					return;
				}
				Test.AddInfo(TEXT("Native companion StateTree completed the same granted held revive GAS action used by humans."));
				Pulse(EKeys::N);
				Enter(EGolemArenaTestPhase::Wave, 90.0);
			}
		}

		void ObserveCombat()
		{
			for (int32 Slot = 1; Slot < 3; ++Slot)
			{
				AGolemCharacter* Golem = Golems[Slot].Get();
				AGolemArenaCompanionController* AI = Cast<AGolemArenaCompanionController>(Golem->GetController());
				if (AI != nullptr && AI->GetDecisionScheduler()->GetDecisionTickCount() > 0)
				{
					bObservedNearLOD |= AI->GetDecisionScheduler()->GetDecisionLOD() == EGolemAILODTier::Near
						&& FMath::IsNearlyEqual(AI->GetDecisionScheduler()->GetDecisionInterval(), 0.1f);
					bObservedFarLOD |= AI->GetDecisionScheduler()->GetDecisionLOD() == EGolemAILODTier::Far
						&& FMath::IsNearlyEqual(AI->GetDecisionScheduler()->GetDecisionInterval(), 0.35f);
				}
				const FGameplayAbilitySpec* Light = Golem->GetGolemAbilitySystem()->FindAbilitySpecFromClass(
					UGolemLightAttackAbility::StaticClass());
				const FGameplayAbilitySpec* Heavy = Golem->GetGolemAbilitySystem()->FindAbilitySpecFromClass(
					UGolemHeavyAttackAbility::StaticClass());
				UAnimInstance* Animation = Golem->GetMesh()->GetAnimInstance();
				bObservedCompanionGAS |= ((Light != nullptr && Light->IsActive()) || (Heavy != nullptr && Heavy->IsActive()))
					&& Animation != nullptr && Animation->Montage_IsPlaying(Golem->GetGolemAbilitySystem()->GetCurrentMontage());
				UGolemCombatComponent* Combat = Golem->FindComponentByClass<UGolemCombatComponent>();
				if (Combat != nullptr)
				{
					CompanionConfirmedHits = FMath::Max(CompanionConfirmedHits,
						Combat->GetSnapshot().GetOutcomeCount(EGolemCombatOutcome::LightHit)
						+ Combat->GetSnapshot().GetOutcomeCount(EGolemCombatOutcome::HeavyHit));
				}
			}
			for (AGolemArenaEnemy* Enemy : Mode->GetEncounterEnemies())
			{
				if (!IsValid(Enemy) || Enemy->GetLifeComponent()->IsDead())
				{
					continue;
				}
				const FGameplayAbilitySpec* Light = Enemy->GetGolemAbilitySystem()->FindAbilitySpecFromClass(
					UGolemLightAttackAbility::StaticClass());
				const FGameplayAbilitySpec* Heavy = Enemy->GetGolemAbilitySystem()->FindAbilitySpecFromClass(
					UGolemHeavyAttackAbility::StaticClass());
				bObservedEnemyGAS |= (Light != nullptr && Light->IsActive()) || (Heavy != nullptr && Heavy->IsActive());
				AGolemEnemyController* AI = Cast<AGolemEnemyController>(Enemy->GetController());
				bObservedPerception |= AI != nullptr && AI->GetRolePerception()->GetEligibleSensedCount() > 0
					&& AI->GetRolePerception()->GetStimulusUpdateCount() > 0 && AI->GetDecisionTickCount() > 0;
				if (State->GetSnapshot().Stage == EGolemArenaStage::Boss && State->GetSnapshot().BossPhase == 1)
				{
					UAnimInstance* Animation = Enemy->GetMesh()->GetAnimInstance();
					const FGolemAIDecisionSnapshot Decision = AI != nullptr
						? AI->GetAIContext()->GetDecisionSnapshot() : FGolemAIDecisionSnapshot();
					bObservedBossHeavy |= Heavy != nullptr && Heavy->IsActive() && Animation != nullptr
						&& Enemy->GetGolemAbilitySystem()->GetCurrentMontage() != nullptr
						&& Animation->Montage_IsPlaying(Enemy->GetGolemAbilitySystem()->GetCurrentMontage())
						&& Decision.bActionRunning && Decision.ActiveActionToken.IsValid()
						&& Decision.ActiveActionTag == GolemGameplayTags::Ability_Combat_Heavy;
				}
			}
			MaximumLeases = FMath::Max(MaximumLeases, Director->GetActiveLeaseCount());
		}

		AGolemArenaEnemy* FindLiveEnemy() const
		{
			for (AGolemArenaEnemy* Enemy : Mode->GetEncounterEnemies())
			{
				if (IsValid(Enemy) && Enemy->GetLifeComponent()->IsAlive())
				{
					return Enemy;
				}
			}
			return nullptr;
		}

		void FightWave()
		{
			if (State->GetSnapshot().Stage == EGolemArenaStage::Boss)
			{
				BossEnemy = FindLiveEnemy();
				if (!BossEnemy.IsValid())
				{
					Fail(TEXT("Wave completion did not spawn a genuine boss."));
					return;
				}
				BossId = BossEnemy->GetArenaEnemyId();
				BossGrantCount = BossEnemy->GetGolemAbilitySystem()->GetOwnedGrantCount();
				BossSpecCount = BossEnemy->GetGolemAbilitySystem()->GetActivatableAbilities().Num();
				bObservedBossFirstPhase = BossEnemy->GetBossPhaseComponent()->GetPhaseIndex() == 0;
				PlaceControlledGolem(BossEnemy->GetActorLocation() - FVector(150.0, 0.0, 0.0), FRotator::ZeroRotator);
				Enter(EGolemArenaTestPhase::Boss, 90.0);
				RecordBossDiagnostic(TEXT("Genuine companion combat continues through phase one; human uses mapped defense"));
				return;
			}
			if (State->GetSnapshot().Stage == EGolemArenaStage::Defeat)
			{
				Fail(TEXT("Party wiped during the mapped combat victory playthrough."));
				return;
			}
			// Let actual companions engage and confirm damage before the human finishes the wave.
			if (WorldElapsed() < 4.0 && (!bObservedCompanionGAS || CompanionConfirmedHits < 1))
			{
				return;
			}
			TryMappedAttack(FindLiveEnemy());
		}

		void TryMappedAttack(AGolemArenaEnemy* Target)
		{
			AGolemCharacter* Golem = Controller->GetControlledGolem();
			UGolemCombatComponent* Combat = Golem->FindComponentByClass<UGolemCombatComponent>();
			if (Target == nullptr || !Golem->GetLifeComponent()->IsAlive() || !Combat->CanTransferControl()
				|| PulseKey.IsValid() || World->GetTimeSeconds() - LastAttackAt < 0.9)
			{
				return;
			}
			PlaceControlledGolem(Target->GetActorLocation() - FVector(110.0, 0.0, 0.0), FRotator::ZeroRotator);
			LastAttackAt = World->GetTimeSeconds();
			Pulse(EKeys::LeftMouseButton);
		}

		bool PrepareBossObservation()
		{
			UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World.Get());
			const TArray<FVector> HoldAnchors =
			{
				FVector(-1500.0, -1500.0, 100.0),
				FVector(1500.0, -1500.0, 100.0)
			};
			for (int32 Slot = 1; Slot < 3; ++Slot)
			{
				AGolemCharacter* Companion = Golems[Slot].Get();
				FNavLocation Floor;
				if (Navigation == nullptr || !Navigation->ProjectPointToNavigation(HoldAnchors[Slot - 1], Floor)
					|| FVector::DistSquared2D(Floor.Location, BossEnemy->GetActorLocation()) < FMath::Square(1000.0))
				{
					Fail(TEXT("Bounded boss observation requires real nav-projected Hold anchors outside combat range."));
					return false;
				}
				// Explicit fixture alignment isolates the phase action without disabling AI or changing its decision data.
				Companion->GetCharacterMovement()->StopMovementImmediately();
				const FVector Location = Floor.Location + FVector(0.0, 0.0,
					Companion->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 3.0f);
				Companion->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
			}
			PlaceControlledGolem(BossEnemy->GetActorLocation() - FVector(150.0, 0.0, 0.0), FRotator::ZeroRotator);
			Pulse(EKeys::Z);
			Test.AddInfo(TEXT("Boss action observation uses explicit nav-projected companion alignment and the actual "
				"mapped Hold command; AI remains running."));
			return true;
		}

		void FightBoss()
		{
			AGolemCharacter* Golem = Controller->GetControlledGolem();
			if (Golem->GetLifeComponent()->IsDead())
			{
				Fail(TEXT("The controlled human died during the mapped boss victory fixture; no revive can continue this run."));
				return;
			}
			if (Golem->GetLifeComponent()->IsDowned())
			{
				bool bHasReviveContinuation = false;
				for (int32 Slot = 1; Slot < 3; ++Slot)
				{
					AGolemCharacter* Companion = Golems[Slot].Get();
					AGolemArenaCompanionController* AI =
						Cast<AGolemArenaCompanionController>(Companion->GetController());
					if (Companion->GetLifeComponent()->IsAlive() && AI != nullptr)
					{
						bHasReviveContinuation |= AI->GetCommandSnapshot().Command != EGolemCompanionCommand::Hold
							|| FVector::DistSquared(Companion->GetActorLocation(), Golem->GetActorLocation())
								<= FMath::Square(Golem->GetLifeComponent()->GetReviveRange());
					}
				}
				if (!bHasReviveContinuation)
				{
					Fail(TEXT("The boss observation has no active companion continuation to revive the controlled human."));
					return;
				}
				if (World->GetTimeSeconds() - LastBossDiagnosticAt >= 5.0)
				{
					RecordBossDiagnostic(TEXT("Controlled human is Downed; awaiting genuine companion revive"));
				}
				return;
			}
			if (State->GetSnapshot().Stage == EGolemArenaStage::Defeat)
			{
				Fail(TEXT("Party wiped during the mapped boss victory playthrough."));
				return;
			}
			if (State->GetSnapshot().Stage == EGolemArenaStage::Victory && !bObservedBossHeavy)
			{
				Fail(TEXT("Boss died before a real phase-two Heavy GAS action and montage were observed."));
				return;
			}
			if (BossEnemy.IsValid())
			{
				if (BossEnemy->GetArenaEnemyId() != BossId
					|| BossEnemy->GetGolemAbilitySystem()->GetOwnedGrantCount() != BossGrantCount
					|| BossEnemy->GetGolemAbilitySystem()->GetActivatableAbilities().Num() != BossSpecCount)
				{
					Fail(TEXT("Boss phase swap changed combat identity or duplicated profile grants."));
					return;
				}
				bObservedBossFirstPhase |= BossEnemy->GetBossPhaseComponent()->GetPhaseIndex() == 0;
				if (BossEnemy->GetBossPhaseComponent()->GetPhaseIndex() == 1)
				{
					bObservedBossSecondPhase = true;
					AGolemEnemyController* AI = Cast<AGolemEnemyController>(BossEnemy->GetController());
					if (AI == nullptr || AI->GetAIContext()->GetActionSetId()
						!= FPrimaryAssetId(TEXT("GolemAIActionSet"), TEXT("ArenaBossPhaseTwo")))
					{
						Fail(TEXT("Real boss health threshold did not swap the native context action-set identity."));
						return;
					}
					if (!bBossObservationPrepared)
					{
						if (!PrepareBossObservation())
						{
							return;
						}
						bBossObservationPrepared = true;
						BossHeavyWaitStarted = World->GetTimeSeconds();
						RecordBossDiagnostic(TEXT("Real phase-two threshold reached by combat; request mapped Hold for observation"));
						return;
					}
					if (!bBossHoldEstablished)
					{
						const double HoldElapsed = World->GetTimeSeconds() - BossHeavyWaitStarted;
						if (HoldElapsed > 5.0)
						{
							Fail(TEXT("The actual mapped Hold command was not acknowledged during phase-two observation."));
							return;
						}
						if (HoldElapsed < 0.3 || !HasCommand(EGolemCompanionCommand::Hold))
						{
							return;
						}
						bBossHoldEstablished = true;
						RecordBossDiagnostic(TEXT("Mapped Hold accepted after the genuine boss health threshold"));
					}
					if (!bObservedBossHeavy)
					{
						if (World->GetTimeSeconds() - BossHeavyWaitStarted > BossHeavyObservationTimeout)
						{
							Fail(TEXT("Phase-two boss did not execute its genuine Heavy action during the bounded defense window."));
							return;
						}
						TryMappedBossDefense();
						if (World->GetTimeSeconds() - LastBossDiagnosticAt >= 5.0)
						{
							RecordBossDiagnostic(TEXT("Awaiting actual phase-two Heavy"));
						}
						return;
					}
					if (!bBossDefenseReleased)
					{
						SetKey(EKeys::Q, false);
						bBossDefenseReleased = true;
						RecordBossDiagnostic(TEXT("Observed the actual phase-two Heavy spec, montage and owned action token"));
						Pulse(EKeys::N);
						return;
					}
				}
				else
				{
					TryMappedBossDefense();
					if (World->GetTimeSeconds() - LastBossDiagnosticAt >= 5.0)
					{
						RecordBossDiagnostic(TEXT("Actual companions fight phase one while the human defends"));
					}
					return;
				}
			}
			if (State->GetSnapshot().Stage == EGolemArenaStage::Victory)
			{
				const int32 HumanHits = Golem->FindComponentByClass<UGolemCombatComponent>()->GetSnapshot()
					.GetOutcomeCount(EGolemCombatOutcome::LightHit);
				if (!bObservedCompanionGAS || CompanionConfirmedHits < 1 || !bObservedEnemyGAS || !bObservedPerception
					|| !bObservedNearLOD || !bObservedFarLOD
					|| !bObservedBossFirstPhase || !bObservedBossSecondPhase || !bObservedBossHeavy
					|| HumanHits < 1 || MaximumLeases < 1 || !HUD->IsDisplayingArenaVictory())
				{
					Fail(TEXT("Mapped victory lacked actual companion/enemy GAS, boss phase action, trace damage or HUD proof."));
					return;
				}
				Test.AddInfo(FString::Printf(TEXT("Real M4 victory: human trace hits%d, companion confirmed hits%d, maxleases%d; two-phase shared GAS boss."),
					HumanHits, CompanionConfirmedHits, MaximumLeases));
				Pulse(EKeys::R);
				Enter(EGolemArenaTestPhase::VictoryRetry);
				return;
			}
			TryMappedAttack(FindLiveEnemy());
		}

		void TryMappedBossDefense()
		{
			AGolemCharacter* Golem = Controller->GetControlledGolem();
			UGolemCombatComponent* Combat = Golem->FindComponentByClass<UGolemCombatComponent>();
			if (!bLegacyGuardFixture)
			{
				// Explicit fixture: this legacy boss observation was written for a block held through the whole
				// fight. The designed Warrior Guard lasts 3 s with a 6 s recovery, so the human's Guard is made
				// unlimited here; the Guard's own limits are checked in Golem.Color.Arena.Playthrough.
				FGolemHeroMoveTuning Moves = Combat->GetHeroMoves();
				Moves.ClassMove = EGolemClassMove::Guard;
				Moves.GuardMaxSeconds = 0.0f;
				Moves.ClassMoveCooldownSeconds = 0.0f;
				Moves.GuardDamageFraction = 0.1f;
				bLegacyGuardFixture = Combat->SetHeroMoves(Moves).IsSuccess();
				Test.AddInfo(TEXT("Legacy fixture: the controlled Golem defends with an unlimited 90% Guard during the boss observation."));
			}
			if (Golem->GetLifeComponent()->IsAlive() && Combat->CanTransferControl()
				&& !DownKeys.Contains(EKeys::Q) && !PulseKey.IsValid())
			{
				// An Enhanced Input Started binding needs one fresh press after the prior attack completes.
				SetKey(EKeys::Q, true);
			}
		}

		void RecordBossDiagnostic(const TCHAR* Reason)
		{
			if (!BossEnemy.IsValid() || !Controller.IsValid())
			{
				return;
			}
			AGolemCharacter* Golem = Controller->GetControlledGolem();
			UGolemCombatComponent* HumanCombatComponent = Golem != nullptr
				? Golem->FindComponentByClass<UGolemCombatComponent>() : nullptr;
			if (Golem == nullptr || HumanCombatComponent == nullptr || Golem->GetGolemAttributeSet() == nullptr
				|| Golem->GetLifeComponent() == nullptr || BossEnemy->GetGolemAttributeSet() == nullptr
				|| BossEnemy->GetLifeComponent() == nullptr || BossEnemy->GetBossPhaseComponent() == nullptr)
			{
				Test.AddInfo(TEXT("Boss diagnostic sources are unavailable during failure cleanup."));
				return;
			}
			LastBossDiagnosticAt = World.IsValid() ? World->GetTimeSeconds() : 0.0;
			AGolemEnemyController* AI = Cast<AGolemEnemyController>(BossEnemy->GetController());
			const bool bHasContext = AI != nullptr && AI->GetAIContext() != nullptr;
			const FGolemAIDecisionSnapshot Decision = bHasContext
				? AI->GetAIContext()->GetDecisionSnapshot() : FGolemAIDecisionSnapshot();
			FGolemAIActionEntry Selected;
			const FGolemResult Selection = bHasContext ? AI->GetAIContext()->TrySelectAction(Selected)
				: FGolemResult::Failure(EGolemError::InvalidState, TEXT("NoBossController"), FText::GetEmpty());
			const FGolemCombatSnapshot HumanCombat = HumanCombatComponent->GetSnapshot();
			const FString Message = FString::Printf(
				TEXT("%s: bossHP%.1f phase%d life%d target%s range%.1f action%s "
					"running%d selectable%d stagger%d tree%d ticks%lld selectContext%s selected%s; "
					"humanHP%.1f life%d block%d attack%d Q%d feedback%s."),
				Reason, BossEnemy->GetGolemAttributeSet()->GetHealth(), BossEnemy->GetBossPhaseComponent()->GetPhaseIndex(),
				static_cast<int32>(BossEnemy->GetLifeComponent()->GetLifeState()),
				*GetNameSafe(Decision.Target.Get()), Decision.TargetDistance, *Decision.ActiveActionTag.ToString(),
				Decision.bActionRunning, Decision.bCanSelectAction, Decision.bStaggered,
				AI != nullptr && AI->GetDecisionTree() != nullptr && AI->GetDecisionTree()->IsRunning(),
				AI != nullptr ? AI->GetDecisionTickCount() : int64(-1),
				*Selection.Context.ToString(), *Selected.ActionTag.ToString(), Golem->GetGolemAttributeSet()->GetHealth(),
				static_cast<int32>(Golem->GetLifeComponent()->GetLifeState()), HumanCombat.bBlocking, HumanCombat.bAttacking,
				DownKeys.Contains(EKeys::Q), *Controller->GetLastCommandResult().Message.ToString());
			Test.AddInfo(Message);
			UE_LOG(LogGolemArenaPlaythrough, Display, TEXT("%s"), *Message);
		}

		bool ValidateReset()
		{
			if (State->GetSnapshot().Stage != EGolemArenaStage::AwaitingStart
				|| !Mode->GetEncounterEnemies().IsEmpty())
			{
				return false;
			}
			if (Director->GetActiveLeaseCount() != 0 || HUD->IsDisplayingArenaVictory() || HUD->IsDisplayingArenaDefeat()
				|| Controller->IsMoveInputIgnored() || Controller->IsLookInputIgnored())
			{
				Fail(TEXT("Retry left an encounter/token/HUD/input state active."));
				return false;
			}
			for (const TWeakObjectPtr<AGolemCharacter>& Entry : Golems)
			{
				AGolemCharacter* Golem = Entry.Get();
				UGolemAbilitySystemComponent* ASC = Golem->GetGolemAbilitySystem();
				if (!Golem->GetLifeComponent()->IsAlive() || Golem->GetLifeComponent()->IsReviveInProgress()
					|| !FMath::IsNearlyEqual(Golem->GetGolemAttributeSet()->GetHealth(), Golem->GetGolemAttributeSet()->GetMaxHealth())
					|| ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Reviving)
					|| ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Attacking)
					|| ASC->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Staggered)
					|| Golem->FindComponentByClass<UGolemCameraModeComponent>()->GetModeCount() != 0)
				{
					Fail(TEXT("Retry did not reset existing canonical GAS life/resources/actions/camera."));
					return false;
				}
				if (AGolemArenaCompanionController* Companion = Cast<AGolemArenaCompanionController>(Golem->GetController()))
				{
					if (!Companion->GetArenaInitializationResult().IsSuccess()
						|| !Companion->GetFollowComponent()->IsRunning()
						|| Companion->GetAIContext()->GetControlledPawn() != Golem
						|| !Companion->GetDecisionScheduler()->HasDecisionTimer())
					{
						Fail(TEXT("Retry did not restore the companion's actual StateTree, owned context and decision timer."));
						return false;
					}
				}
			}
			return ValidateCanonicalParty();
		}

		void InjectPartyWipe()
		{
			AGolemArenaEnemy* Source = FindLiveEnemy();
			if (Source == nullptr)
			{
				Fail(TEXT("Wipe fixture has no real hostile GAS source."));
				return;
			}
			for (const TWeakObjectPtr<AGolemCharacter>& Golem : Golems)
			{
				if (!UGolemAbilitySystemComponent::ApplyDamage(Source, Golem.Get(), 1000.0f).IsSuccess())
				{
					Fail(TEXT("Explicit GAS wipe fault injection was rejected."));
					return;
				}
			}
			Enter(EGolemArenaTestPhase::Wipe);
		}

		void AbortOwnedQuery()
		{
			if (QueryObservation.IsValid())
			{
				QueryObservation->bAcceptCallbacks = false;
			}
			if (OwnedQueryId != INDEX_NONE && World.IsValid())
			{
				if (UEnvQueryManager* Manager = UEnvQueryManager::GetCurrent(World.Get()))
				{
					Manager->AbortQuery(OwnedQueryId);
				}
			}
			OwnedQueryId = INDEX_NONE;
			QueryObservation.Reset();
		}

		void Fail(const FString& Message)
		{
			if (Phase == EGolemArenaTestPhase::Cleanup || Phase == EGolemArenaTestPhase::Complete)
			{
				return;
			}
			if (State.IsValid())
			{
				const FGolemPartyArenaSnapshot Snapshot = State->GetSnapshot();
				Test.AddInfo(FString::Printf(TEXT("Arena failure phase%d elapsed%.2f stage%d revision%d enemies%d bossphase%d alive%d downed%d leases%d; companionGAS%d hits%d revive%d enemyGAS%d boss0%d boss1%d heavy%d."),
					static_cast<int32>(Phase), WorldElapsed(), static_cast<int32>(Snapshot.Stage), Snapshot.Revision,
					Snapshot.AliveEnemies, Snapshot.BossPhase, Snapshot.AliveGolems, Snapshot.DownedGolems,
					Director.IsValid() ? Director->GetActiveLeaseCount() : -1, bObservedCompanionGAS,
					CompanionConfirmedHits, bObservedCompanionRevive, bObservedEnemyGAS,
					bObservedBossFirstPhase, bObservedBossSecondPhase, bObservedBossHeavy));
			}
			if (QueryObservation.IsValid())
			{
				Test.AddInfo(FString::Printf(TEXT("EQS observation complete%d success%d aborted%d callbacks%d items%d location%s."),
					QueryObservation->bCompleted, QueryObservation->bSucceeded, QueryObservation->bAborted,
					QueryObservation->CallbackCount, QueryObservation->ResultCount, *QueryObservation->Location.ToString()));
			}
			RecordBossDiagnostic(TEXT("Arena failure evidence"));
			Test.AddError(Message);
			BeginCleanup();
		}

		void BeginCleanup()
		{
			if (Phase == EGolemArenaTestPhase::Cleanup || Phase == EGolemArenaTestPhase::Complete)
			{
				return;
			}
			AbortOwnedQuery();
			ReleaseInput();
			Enter(EGolemArenaTestPhase::Cleanup, 15.0);
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

		void RestoreSettings()
		{
			if (Settings.IsValid())
			{
				ULevelEditorPlaySettings* Defaults = GetMutableDefault<ULevelEditorPlaySettings>();
				Defaults->MultipleInstancePositions = OriginalPositions;
				Defaults->LastSize = OriginalLastSize;
				Defaults->NewWindowPosition = OriginalPosition;
				Defaults->NewWindowWidth = OriginalWidth;
				Defaults->NewWindowHeight = OriginalHeight;
				Defaults->SaveConfig();
			}
		}

		void Cleanup()
		{
			if (bOwnsSession && GEditor != nullptr && GEditor->IsPlaySessionInProgress())
			{
				if (FPlatformTime::Seconds() - PhaseStarted < 12.0)
				{
					return;
				}
				Test.AddError(TEXT("Arena PIE cleanup exceeded its bound."));
				if (GEditor->IsPlayingSessionInEditor())
				{
					GEditor->EndPlayMap();
				}
			}
			RestoreSettings();
			Settings.Reset();
			bOwnsSession = false;
			Enter(EGolemArenaTestPhase::Complete);
		}

		FAutomationTestBase& Test;
		EGolemArenaTestPhase Phase = EGolemArenaTestPhase::Navigation;
		double RunStarted = 0.0;
		double PhaseStarted = 0.0;
		double PhaseWorldStarted = 0.0;
		double PhaseTimeout = 30.0;
		double ReadyWorldTime = -1.0;
		double PulseStarted = 0.0;
		double LastAttackAt = -10.0;
		double BossHeavyWaitStarted = -1.0;
		double LastBossDiagnosticAt = -10.0;
		bool bOwnsSession = false;
		bool bObservedCompanionGAS = false;
		bool bObservedCompanionRevive = false;
		bool bObservedEnemyGAS = false;
		bool bObservedPerception = false;
		bool bObservedNearLOD = false;
		bool bObservedFarLOD = false;
		bool bObservedBossFirstPhase = false;
		bool bObservedBossSecondPhase = false;
		bool bObservedBossHeavy = false;
		bool bBossHoldEstablished = false;
		bool bBossObservationPrepared = false;
		bool bBossDefenseReleased = false;
		bool bLegacyGuardFixture = false;
		int32 CompanionConfirmedHits = 0;
		int32 MaximumLeases = 0;
		int32 BossGrantCount = 0;
		int32 BossSpecCount = 0;
		int32 OwnedQueryId = INDEX_NONE;
		FGolemEntityId BossId;
		FKey PulseKey;
		TSet<FKey> DownKeys;
		TArray<double> FollowDistances;
		TArray<TWeakObjectPtr<AGolemCharacter>> Golems;
		TArray<FGolemEntityId> GolemIds;
		TArray<FPrimaryAssetId> HeroIds;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AGolemPartyArenaGameMode> Mode;
		TWeakObjectPtr<AGolemArenaPlayerController> Controller;
		TWeakObjectPtr<AGolemPartyState> Party;
		TWeakObjectPtr<AGolemPartyArenaState> State;
		TWeakObjectPtr<AGolemPartyArenaHUD> HUD;
		TWeakObjectPtr<UGolemCombatDirectorSubsystem> Director;
		TWeakObjectPtr<AGolemArenaEnemy> QueryEnemy;
		TWeakObjectPtr<AGolemEnemyController> QueryController;
		TWeakObjectPtr<AGolemArenaEnemy> BossEnemy;
		TWeakObjectPtr<UEnvQuery> QueryTemplate;
		FTransform QueryEnemyTransform;
		FVector HumanMovementOrigin = FVector::ZeroVector;
		TSharedPtr<FGolemArenaQueryObservation> QueryObservation;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize = FIntPoint::ZeroValue;
		FIntPoint OriginalPosition = FIntPoint::ZeroValue;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		FWaitForEditorNavMeshBuildCommand NavigationWait;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemPartyArenaPlaythroughTest, "Golem.AI.Arena.Playthrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemPartyArenaPlaythroughTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->IsPlaySessionInProgress()
		|| !FPackageName::DoesPackageExist(GolemPartyArenaTest::MapPath)
		|| !FEditorFileUtils::LoadMap(GolemPartyArenaTest::MapPath, false, false))
	{
		AddError(TEXT("Run GolemBootstrap -Milestone=4 on the integrated build and use an idle Editor."));
		return false;
	}
	AddInfo(TEXT("M4 uses actual mapped movement/commands/start/hammer/block/retry, shared StateTree/GAS and asynchronous EQS. Placement/down/wipe are explicit bounded negative-case fixture setup."));
	AddCommand(new GolemPartyArenaTest::FGolemArenaScenario(*this));
	return true;
}

#endif
