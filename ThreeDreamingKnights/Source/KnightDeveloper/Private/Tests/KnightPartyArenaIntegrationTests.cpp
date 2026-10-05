#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Abilities/KnightHeavyAttackAbility.h"
#include "Abilities/KnightLightAttackAbility.h"
#include "Abilities/KnightReviveAbility.h"
#include "Attributes/KnightAttributeSet.h"
#include "Boss/KnightBossPhaseComponent.h"
#include "Camera/KnightCameraModeComponent.h"
#include "Character/KnightCharacter.h"
#include "Combat/KnightCombatDirectorSubsystem.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightAIDecisionSchedulerComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Context/KnightAIContextComponent.h"
#include "Controllers/KnightArenaCompanionController.h"
#include "Controllers/KnightEnemyController.h"
#include "Definitions/KnightAIActionSetDefinition.h"
#include "Party/KnightPartyState.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "PartyArena/KnightArenaPlayerController.h"
#include "PartyArena/KnightPartyArenaGameMode.h"
#include "PartyArena/KnightPartyArenaHUD.h"
#include "PartyArena/KnightPartyArenaState.h"
#include "Perception/KnightAIRolePerceptionComponent.h"
#include "Tags/KnightGameplayTags.h"

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

DEFINE_LOG_CATEGORY_STATIC(LogKnightArenaPlaythrough, Log, All);

namespace KnightPartyArenaTest
{
	const TCHAR* MapPath = TEXT("/Game/Knight/Maps/Playables/L_M04_PartyArena");
	constexpr double BossHeavyObservationTimeout = 15.0;

	enum class EKnightArenaTestPhase : uint8
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
		LockTarget,
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

	struct FKnightArenaQueryObservation
	{
		bool bAcceptCallbacks = true;
		bool bCompleted = false;
		bool bSucceeded = false;
		bool bAborted = false;
		int32 CallbackCount = 0;
		int32 ResultCount = 0;
		FVector Location = FVector::ZeroVector;
	};

	class FKnightArenaScenario final : public IAutomationLatentCommand
	{
	public:
		explicit FKnightArenaScenario(FAutomationTestBase& InTest)
			: Test(InTest), RunStarted(FPlatformTime::Seconds()), PhaseStarted(RunStarted)
		{
		}

		virtual ~FKnightArenaScenario() override
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
			if (Phase == EKnightArenaTestPhase::Complete)
			{
				return true;
			}
			if (Phase != EKnightArenaTestPhase::Cleanup
				&& (FPlatformTime::Seconds() - RunStarted > 300.0
					|| FPlatformTime::Seconds() - PhaseStarted > PhaseTimeout))
			{
				Fail(TEXT("Bounded real arena playthrough phase timed out."));
			}
			if (Phase != EKnightArenaTestPhase::Cleanup && Test.HasAnyErrors())
			{
				BeginCleanup();
			}
			if (World.IsValid() && Phase > EKnightArenaTestPhase::Ready && Phase < EKnightArenaTestPhase::Cleanup)
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
			case EKnightArenaTestPhase::Navigation:
				if (NavigationWait.Update())
				{
					Enter(EKnightArenaTestPhase::Start, 30.0);
				}
				break;
			case EKnightArenaTestPhase::Start:
				Start();
				break;
			case EKnightArenaTestPhase::Ready:
				Ready();
				break;
			case EKnightArenaTestPhase::FollowObstacle:
				FollowObstacle();
				break;
			case EKnightArenaTestPhase::Hold:
				if (WorldElapsed() > 0.4 && HasCommand(EKnightCompanionCommand::Hold))
				{
					Pulse(EKeys::K);
					Enter(EKnightArenaTestPhase::Regroup);
				}
				break;
			case EKnightArenaTestPhase::Regroup:
				if (WorldElapsed() > 0.4 && HasCommand(EKnightCompanionCommand::Regroup))
				{
					PlaceControlledKnight(FVector(0.0, -820.0, 100.0), FRotator(0.0, 90.0, 0.0));
					Pulse(EKeys::F);
					Enter(EKnightArenaTestPhase::StartEncounter);
				}
				break;
			case EKnightArenaTestPhase::StartEncounter:
				if (State->GetSnapshot().Stage == EKnightArenaStage::Wave && Mode->GetEncounterEnemies().Num() == 2)
				{
					BeginQueryChecks();
				}
				break;
			case EKnightArenaTestPhase::QueryPositive:
				CheckPositiveQuery();
				break;
			case EKnightArenaTestPhase::QueryUnreachable:
				CheckUnreachableQuery();
				break;
			case EKnightArenaTestPhase::QueryCancel:
				CheckCancelledQuery();
				break;
			case EKnightArenaTestPhase::LockTarget:
				if (!PulseKey.IsValid() && Controller->GetControlledKnight()->FindComponentByClass<UKnightCombatComponent>()
					->GetLockedTarget() == QueryEnemy.Get())
				{
					Pulse(EKeys::B);
					Enter(EKnightArenaTestPhase::FocusCommand);
				}
				break;
			case EKnightArenaTestPhase::FocusCommand:
				if (WorldElapsed() > 0.4 && HasCommand(EKnightCompanionCommand::Focus))
				{
					for (int32 Slot = 1; Slot < 3; ++Slot)
					{
						AKnightArenaCompanionController* AI = CastChecked<AKnightArenaCompanionController>(
							Knights[Slot]->GetController());
						if (AI->GetCommandSnapshot().FocusTargetId != QueryEnemy->GetArenaEnemyId())
						{
							Fail(TEXT("Mapped Focus command did not preserve the exact validated hostile target identity."));
							return false;
						}
					}
					BeginCompanionRevive();
				}
				break;
			case EKnightArenaTestPhase::CompanionRevive:
				CheckCompanionRevive();
				break;
			case EKnightArenaTestPhase::Wave:
				FightWave();
				break;
			case EKnightArenaTestPhase::Boss:
				FightBoss();
				break;
			case EKnightArenaTestPhase::VictoryRetry:
				if (WorldElapsed() > 0.6 && ValidateReset())
				{
					PlaceControlledKnight(FVector(0.0, -820.0, 100.0), FRotator(0.0, 90.0, 0.0));
					Pulse(EKeys::F);
					Enter(EKnightArenaTestPhase::WipeStart);
				}
				break;
			case EKnightArenaTestPhase::WipeStart:
				if (State->GetSnapshot().Stage == EKnightArenaStage::Wave && Mode->GetEncounterEnemies().Num() == 2)
				{
					InjectPartyWipe();
				}
				break;
			case EKnightArenaTestPhase::Wipe:
				if (WorldElapsed() > 0.3 && State->GetSnapshot().Stage == EKnightArenaStage::Defeat)
				{
					if (!HUD->IsDisplayingArenaDefeat() || Director->GetActiveLeaseCount() != 0)
					{
						Fail(TEXT("Real wipe did not display Defeat and release active attack leases."));
						break;
					}
					Pulse(EKeys::R);
					Enter(EKnightArenaTestPhase::DefeatRetry);
				}
				break;
			case EKnightArenaTestPhase::DefeatRetry:
				if (WorldElapsed() > 0.6 && ValidateReset())
				{
					Test.AddInfo(TEXT("Actual M4 mapped victory and wipe both reset stable actors, grants, tokens, life and input."));
					BeginCleanup();
				}
				break;
			case EKnightArenaTestPhase::Cleanup:
				Cleanup();
				break;
			default:
				break;
			}
			return Phase == EKnightArenaTestPhase::Complete;
		}

	private:
		void Enter(const EKnightArenaTestPhase Next, const double Timeout = 15.0)
		{
			Phase = Next;
			PhaseStarted = FPlatformTime::Seconds();
			PhaseWorldStarted = World.IsValid() ? World->GetTimeSeconds() : 0.0;
			PhaseTimeout = Timeout;
			Test.AddInfo(FString::Printf(TEXT("Arena phase %d started."), static_cast<int32>(Phase)));
			UE_LOG(LogKnightArenaPlaythrough, Display, TEXT("Actual arena phase%d started."), static_cast<int32>(Phase));
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
			Enter(EKnightArenaTestPhase::Ready, 35.0);
		}

		void Ready()
		{
			UWorld* PIEWorld = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
			if (PIEWorld == nullptr || !PIEWorld->HasBegunPlay())
			{
				return;
			}
			World = PIEWorld;
			Mode = PIEWorld->GetAuthGameMode<AKnightPartyArenaGameMode>();
			Controller = Cast<AKnightArenaPlayerController>(PIEWorld->GetFirstPlayerController());
			Party = Mode.IsValid() ? Mode->GetPartyState() : nullptr;
			State = Mode.IsValid() ? Mode->GetArenaState() : nullptr;
			HUD = Controller.IsValid() ? Cast<AKnightPartyArenaHUD>(Controller->GetHUD()) : nullptr;
			Director = PIEWorld->GetSubsystem<UKnightCombatDirectorSubsystem>();
			if (!Party.IsValid() || !State.IsValid() || !Controller.IsValid() || !HUD.IsValid() || !Director.IsValid()
				|| !Party->IsCanonicalRosterReady() || Controller->GetControlledKnight() == nullptr)
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
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || Entry->Knight == nullptr)
				{
					Fail(TEXT("Canonical arena roster was not fully initialized."));
					return;
				}
				Knights.Add(Entry->Knight.Get());
				KnightIds.Add(Entry->KnightId);
				HeroIds.Add(Entry->HeroDefinitionId);
			}
			if (!ValidateCanonicalParty() || State->GetSnapshot().Stage != EKnightArenaStage::AwaitingStart)
			{
				Fail(TEXT("Arena entry did not expose exactly three configured Knights and AwaitingStart."));
				return;
			}
			for (int32 Slot = 1; Slot < 3; ++Slot)
			{
				AKnightArenaCompanionController* AI = Cast<AKnightArenaCompanionController>(Knights[Slot]->GetController());
				if (AI == nullptr || !AI->GetArenaInitializationResult().IsSuccess()
					|| !AI->GetDecisionScheduler()->HasDecisionTimer())
				{
					Fail(TEXT("Arena companion did not run a real native StateTree decision scheduler."));
					return;
				}
				FollowDistances.Add(FVector::Dist2D(Knights[Slot]->GetActorLocation(), FVector(-300.0, 350.0, 100.0)));
			}
			PlaceControlledKnight(FVector(-300.0, 350.0, 100.0), FRotator(0.0, 90.0, 0.0));
			HumanMovementOrigin = Controller->GetControlledKnight()->GetActorLocation();
			SetKey(EKeys::W, true);
			Enter(EKnightArenaTestPhase::FollowObstacle, 15.0);
		}

		bool ValidateCanonicalParty()
		{
			if (!World.IsValid() || !Party.IsValid() || !Controller.IsValid() || !State.IsValid() || Knights.Num() != 3)
			{
				Fail(TEXT("Arena lost its live canonical party."));
				return false;
			}
			int32 Count = 0;
			for (TActorIterator<AKnightCharacter> Iterator(World.Get()); Iterator; ++Iterator)
			{
				++Count;
			}
			if (Count != 3 || Party->GetHumanCount() != 1)
			{
				Fail(TEXT("Arena created an extra canonical Knight or changed solo human ownership."));
				return false;
			}
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || Entry->Knight != Knights[Slot].Get() || Entry->KnightId != KnightIds[Slot]
					|| Entry->HeroDefinitionId != HeroIds[Slot]
					|| Entry->Knight->GetKnightAbilitySystem()->GetOwnedGrantCount() != 2
					|| Entry->Knight->GetKnightAbilitySystem()->GetActivatableAbilities().Num() != 11)
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
			for (const FKnightEntityId& Id : KnightIds)
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
			if (FVector::Dist2D(HumanMovementOrigin, Controller->GetControlledKnight()->GetActorLocation()) < 50.0)
			{
				Fail(TEXT("Mapped W did not move the controlled Knight in the real CharacterMovement world."));
				return;
			}
			for (int32 Slot = 1; Slot < 3; ++Slot)
			{
				const double Remaining = FVector::Dist2D(Knights[Slot]->GetActorLocation(),
					Controller->GetControlledKnight()->GetActorLocation());
				if (Remaining > FollowDistances[Slot - 1] - 400.0)
				{
					return;
				}
			}
			Test.AddInfo(TEXT("Both native companions followed real nav paths across the arena LOS obstacle."));
			Pulse(EKeys::Z);
			Enter(EKnightArenaTestPhase::Hold);
		}

		bool HasCommand(const EKnightCompanionCommand Command)
		{
			if (!Controller->GetLastCompanionCommandResult().IsSuccess()
				|| Controller->GetLastCompanionCommand() != Command || HUD->GetDisplayedCommandFeedback().IsEmpty())
			{
				return false;
			}
			for (int32 Slot = 1; Slot < 3; ++Slot)
			{
				AKnightArenaCompanionController* AI = Cast<AKnightArenaCompanionController>(Knights[Slot]->GetController());
				if (AI == nullptr || AI->GetCommandSnapshot().Command != Command
					|| AI->GetCommandSnapshot().CommandRevision < 1)
				{
					return false;
				}
			}
			return true;
		}

		void PlaceControlledKnight(const FVector& Location, const FRotator& Rotation)
		{
			ReleaseInput();
			AKnightCharacter* Knight = Controller->GetControlledKnight();
			Knight->GetCharacterMovement()->StopMovementImmediately();
			Knight->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
			Controller->SetControlRotation(Rotation);
		}

		void BeginQueryChecks()
		{
			QueryEnemy = Mode->GetEncounterEnemies()[0].Get();
			QueryController = QueryEnemy.IsValid() ? Cast<AKnightEnemyController>(QueryEnemy->GetController()) : nullptr;
			if (!QueryController.IsValid() || !QueryController->GetInitializationResult().IsSuccess()
				|| QueryController->GetDecisionTree() == nullptr || !QueryController->GetDecisionTree()->IsRunning())
			{
				Fail(TEXT("Wave enemy did not initialize a genuine running StateTree."));
				return;
			}
			QueryEnemyTransform = QueryEnemy->GetActorTransform();
			QueryController->GetDecisionTree()->StopLogic(TEXT("Explicit bounded EQS negative-case fixture"));
			QueryController->StopMovement();
			if (!QueryController->GetAIContext()->SetValidatedTarget(Controller->GetControlledKnight()).IsSuccess())
			{
				Fail(TEXT("Real EQS querier rejected the validated canonical target."));
				return;
			}
			QueryTemplate = QueryController->GetAIContext()->GetRepositionQuery();
			if (!StartOwnedQuery())
			{
				return;
			}
			Enter(EKnightArenaTestPhase::QueryPositive);
		}

		bool StartOwnedQuery()
		{
			QueryObservation = MakeShared<FKnightArenaQueryObservation>();
			TWeakPtr<FKnightArenaQueryObservation> WeakObservation = QueryObservation;
			FQueryFinishedSignature Callback = FQueryFinishedSignature::CreateLambda(
				[WeakObservation](TSharedPtr<FEnvQueryResult> Result)
				{
					TSharedPtr<FKnightArenaQueryObservation> Observation = WeakObservation.Pin();
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
			AKnightCharacter* Target = Controller->GetControlledKnight();
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
				Enter(EKnightArenaTestPhase::QueryUnreachable);
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
			Enter(EKnightArenaTestPhase::QueryCancel);
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
			PlaceControlledKnight(QueryEnemy->GetActorLocation() - FVector(0.0, 300.0, 0.0),
				FRotator(0.0, 90.0, 0.0));
			Pulse(EKeys::MiddleMouseButton);
			Enter(EKnightArenaTestPhase::LockTarget);
		}

		void BeginCompanionRevive()
		{
			AKnightCharacter* Downed = Knights[1].Get();
			AKnightCharacter* Reviver = Knights[2].Get();
			Downed->SetActorLocation(FVector(-950.0, -450.0, 100.0), false, nullptr, ETeleportType::TeleportPhysics);
			Reviver->SetActorLocation(FVector(-1080.0, -450.0, 100.0), false, nullptr, ETeleportType::TeleportPhysics);
			PlaceControlledKnight(FVector(500.0, -850.0, 100.0), FRotator::ZeroRotator);
			Pulse(EKeys::Z);
			if (!UKnightAbilitySystemComponent::ApplyDamage(QueryEnemy.Get(), Downed, 1000.0f).IsSuccess()
				|| !Downed->GetLifeComponent()->IsDowned())
			{
				Fail(TEXT("Real GAS downed fault did not initialize the companion revive scenario."));
				return;
			}
			Enter(EKnightArenaTestPhase::CompanionRevive, 20.0);
		}

		void CheckCompanionRevive()
		{
			AKnightCharacter* Downed = Knights[1].Get();
			AKnightCharacter* Reviver = Knights[2].Get();
			const FGameplayAbilitySpec* ReviveSpec = Reviver->GetKnightAbilitySystem()->FindAbilitySpecFromClass(
				UKnightReviveAbility::StaticClass());
			if (Downed->GetLifeComponent()->IsReviveInProgress()
				&& Downed->GetLifeComponent()->GetActiveReviver() == Reviver && ReviveSpec != nullptr && ReviveSpec->IsActive()
				&& Reviver->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving))
			{
				bObservedCompanionRevive = true;
			}
			if (Downed->GetLifeComponent()->IsAlive())
			{
				if (!bObservedCompanionRevive || Downed->GetLifeComponent()->IsReviveInProgress()
					|| ReviveSpec == nullptr || ReviveSpec->IsActive()
					|| Reviver->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving))
				{
					Fail(TEXT("Companion revive did not execute and clean the actual shared GAS ability/reservation."));
					return;
				}
				Test.AddInfo(TEXT("Native companion StateTree completed the same granted held revive GAS action used by humans."));
				Pulse(EKeys::N);
				Enter(EKnightArenaTestPhase::Wave, 90.0);
			}
		}

		void ObserveCombat()
		{
			for (int32 Slot = 1; Slot < 3; ++Slot)
			{
				AKnightCharacter* Knight = Knights[Slot].Get();
				AKnightArenaCompanionController* AI = Cast<AKnightArenaCompanionController>(Knight->GetController());
				if (AI != nullptr && AI->GetDecisionScheduler()->GetDecisionTickCount() > 0)
				{
					bObservedNearLOD |= AI->GetDecisionScheduler()->GetDecisionLOD() == EKnightAILODTier::Near
						&& FMath::IsNearlyEqual(AI->GetDecisionScheduler()->GetDecisionInterval(), 0.1f);
					bObservedFarLOD |= AI->GetDecisionScheduler()->GetDecisionLOD() == EKnightAILODTier::Far
						&& FMath::IsNearlyEqual(AI->GetDecisionScheduler()->GetDecisionInterval(), 0.35f);
				}
				const FGameplayAbilitySpec* Light = Knight->GetKnightAbilitySystem()->FindAbilitySpecFromClass(
					UKnightLightAttackAbility::StaticClass());
				const FGameplayAbilitySpec* Heavy = Knight->GetKnightAbilitySystem()->FindAbilitySpecFromClass(
					UKnightHeavyAttackAbility::StaticClass());
				UAnimInstance* Animation = Knight->GetMesh()->GetAnimInstance();
				bObservedCompanionGAS |= ((Light != nullptr && Light->IsActive()) || (Heavy != nullptr && Heavy->IsActive()))
					&& Animation != nullptr && Animation->Montage_IsPlaying(Knight->GetKnightAbilitySystem()->GetCurrentMontage());
				UKnightCombatComponent* Combat = Knight->FindComponentByClass<UKnightCombatComponent>();
				if (Combat != nullptr)
				{
					CompanionConfirmedHits = FMath::Max(CompanionConfirmedHits,
						Combat->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::LightHit)
						+ Combat->GetSnapshot().GetOutcomeCount(EKnightCombatOutcome::HeavyHit));
				}
			}
			for (AKnightArenaEnemy* Enemy : Mode->GetEncounterEnemies())
			{
				if (!IsValid(Enemy) || Enemy->GetLifeComponent()->IsDead())
				{
					continue;
				}
				const FGameplayAbilitySpec* Light = Enemy->GetKnightAbilitySystem()->FindAbilitySpecFromClass(
					UKnightLightAttackAbility::StaticClass());
				const FGameplayAbilitySpec* Heavy = Enemy->GetKnightAbilitySystem()->FindAbilitySpecFromClass(
					UKnightHeavyAttackAbility::StaticClass());
				bObservedEnemyGAS |= (Light != nullptr && Light->IsActive()) || (Heavy != nullptr && Heavy->IsActive());
				AKnightEnemyController* AI = Cast<AKnightEnemyController>(Enemy->GetController());
				bObservedPerception |= AI != nullptr && AI->GetRolePerception()->GetEligibleSensedCount() > 0
					&& AI->GetRolePerception()->GetStimulusUpdateCount() > 0 && AI->GetDecisionTickCount() > 0;
				if (State->GetSnapshot().Stage == EKnightArenaStage::Boss && State->GetSnapshot().BossPhase == 1)
				{
					UAnimInstance* Animation = Enemy->GetMesh()->GetAnimInstance();
					const FKnightAIDecisionSnapshot Decision = AI != nullptr
						? AI->GetAIContext()->GetDecisionSnapshot() : FKnightAIDecisionSnapshot();
					bObservedBossHeavy |= Heavy != nullptr && Heavy->IsActive() && Animation != nullptr
						&& Enemy->GetKnightAbilitySystem()->GetCurrentMontage() != nullptr
						&& Animation->Montage_IsPlaying(Enemy->GetKnightAbilitySystem()->GetCurrentMontage())
						&& Decision.bActionRunning && Decision.ActiveActionToken.IsValid()
						&& Decision.ActiveActionTag == KnightGameplayTags::Ability_Combat_Heavy;
				}
			}
			MaximumLeases = FMath::Max(MaximumLeases, Director->GetActiveLeaseCount());
		}

		AKnightArenaEnemy* FindLiveEnemy() const
		{
			for (AKnightArenaEnemy* Enemy : Mode->GetEncounterEnemies())
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
			if (State->GetSnapshot().Stage == EKnightArenaStage::Boss)
			{
				BossEnemy = FindLiveEnemy();
				if (!BossEnemy.IsValid())
				{
					Fail(TEXT("Wave completion did not spawn a genuine boss."));
					return;
				}
				BossId = BossEnemy->GetArenaEnemyId();
				BossGrantCount = BossEnemy->GetKnightAbilitySystem()->GetOwnedGrantCount();
				BossSpecCount = BossEnemy->GetKnightAbilitySystem()->GetActivatableAbilities().Num();
				bObservedBossFirstPhase = BossEnemy->GetBossPhaseComponent()->GetPhaseIndex() == 0;
				PlaceControlledKnight(BossEnemy->GetActorLocation() - FVector(150.0, 0.0, 0.0), FRotator::ZeroRotator);
				Enter(EKnightArenaTestPhase::Boss, 90.0);
				RecordBossDiagnostic(TEXT("Genuine companion combat continues through phase one; human uses mapped defense"));
				return;
			}
			if (State->GetSnapshot().Stage == EKnightArenaStage::Defeat)
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

		void TryMappedAttack(AKnightArenaEnemy* Target)
		{
			AKnightCharacter* Knight = Controller->GetControlledKnight();
			UKnightCombatComponent* Combat = Knight->FindComponentByClass<UKnightCombatComponent>();
			if (Target == nullptr || !Knight->GetLifeComponent()->IsAlive() || !Combat->CanTransferControl()
				|| PulseKey.IsValid() || World->GetTimeSeconds() - LastAttackAt < 0.9)
			{
				return;
			}
			PlaceControlledKnight(Target->GetActorLocation() - FVector(110.0, 0.0, 0.0), FRotator::ZeroRotator);
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
				AKnightCharacter* Companion = Knights[Slot].Get();
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
			PlaceControlledKnight(BossEnemy->GetActorLocation() - FVector(150.0, 0.0, 0.0), FRotator::ZeroRotator);
			Pulse(EKeys::Z);
			Test.AddInfo(TEXT("Boss action observation uses explicit nav-projected companion alignment and the actual "
				"mapped Hold command; AI remains running."));
			return true;
		}

		void FightBoss()
		{
			AKnightCharacter* Knight = Controller->GetControlledKnight();
			if (Knight->GetLifeComponent()->IsDead())
			{
				Fail(TEXT("The controlled human died during the mapped boss victory fixture; no revive can continue this run."));
				return;
			}
			if (Knight->GetLifeComponent()->IsDowned())
			{
				bool bHasReviveContinuation = false;
				for (int32 Slot = 1; Slot < 3; ++Slot)
				{
					AKnightCharacter* Companion = Knights[Slot].Get();
					AKnightArenaCompanionController* AI =
						Cast<AKnightArenaCompanionController>(Companion->GetController());
					if (Companion->GetLifeComponent()->IsAlive() && AI != nullptr)
					{
						bHasReviveContinuation |= AI->GetCommandSnapshot().Command != EKnightCompanionCommand::Hold
							|| FVector::DistSquared(Companion->GetActorLocation(), Knight->GetActorLocation())
								<= FMath::Square(Knight->GetLifeComponent()->GetReviveRange());
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
			if (State->GetSnapshot().Stage == EKnightArenaStage::Defeat)
			{
				Fail(TEXT("Party wiped during the mapped boss victory playthrough."));
				return;
			}
			if (State->GetSnapshot().Stage == EKnightArenaStage::Victory && !bObservedBossHeavy)
			{
				Fail(TEXT("Boss died before a real phase-two Heavy GAS action and montage were observed."));
				return;
			}
			if (BossEnemy.IsValid())
			{
				if (BossEnemy->GetArenaEnemyId() != BossId
					|| BossEnemy->GetKnightAbilitySystem()->GetOwnedGrantCount() != BossGrantCount
					|| BossEnemy->GetKnightAbilitySystem()->GetActivatableAbilities().Num() != BossSpecCount)
				{
					Fail(TEXT("Boss phase swap changed combat identity or duplicated profile grants."));
					return;
				}
				bObservedBossFirstPhase |= BossEnemy->GetBossPhaseComponent()->GetPhaseIndex() == 0;
				if (BossEnemy->GetBossPhaseComponent()->GetPhaseIndex() == 1)
				{
					bObservedBossSecondPhase = true;
					AKnightEnemyController* AI = Cast<AKnightEnemyController>(BossEnemy->GetController());
					if (AI == nullptr || AI->GetAIContext()->GetActionSetId()
						!= FPrimaryAssetId(TEXT("KnightAIActionSet"), TEXT("ArenaBossPhaseTwo")))
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
						if (HoldElapsed < 0.3 || !HasCommand(EKnightCompanionCommand::Hold))
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
			if (State->GetSnapshot().Stage == EKnightArenaStage::Victory)
			{
				const int32 HumanHits = Knight->FindComponentByClass<UKnightCombatComponent>()->GetSnapshot()
					.GetOutcomeCount(EKnightCombatOutcome::LightHit);
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
				Enter(EKnightArenaTestPhase::VictoryRetry);
				return;
			}
			TryMappedAttack(FindLiveEnemy());
		}

		void TryMappedBossDefense()
		{
			AKnightCharacter* Knight = Controller->GetControlledKnight();
			UKnightCombatComponent* Combat = Knight->FindComponentByClass<UKnightCombatComponent>();
			if (Knight->GetLifeComponent()->IsAlive() && Combat->CanTransferControl()
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
			AKnightCharacter* Knight = Controller->GetControlledKnight();
			UKnightCombatComponent* HumanCombatComponent = Knight != nullptr
				? Knight->FindComponentByClass<UKnightCombatComponent>() : nullptr;
			if (Knight == nullptr || HumanCombatComponent == nullptr || Knight->GetKnightAttributeSet() == nullptr
				|| Knight->GetLifeComponent() == nullptr || BossEnemy->GetKnightAttributeSet() == nullptr
				|| BossEnemy->GetLifeComponent() == nullptr || BossEnemy->GetBossPhaseComponent() == nullptr)
			{
				Test.AddInfo(TEXT("Boss diagnostic sources are unavailable during failure cleanup."));
				return;
			}
			LastBossDiagnosticAt = World.IsValid() ? World->GetTimeSeconds() : 0.0;
			AKnightEnemyController* AI = Cast<AKnightEnemyController>(BossEnemy->GetController());
			const bool bHasContext = AI != nullptr && AI->GetAIContext() != nullptr;
			const FKnightAIDecisionSnapshot Decision = bHasContext
				? AI->GetAIContext()->GetDecisionSnapshot() : FKnightAIDecisionSnapshot();
			FKnightAIActionEntry Selected;
			const FKnightResult Selection = bHasContext ? AI->GetAIContext()->TrySelectAction(Selected)
				: FKnightResult::Failure(EKnightError::InvalidState, TEXT("NoBossController"), FText::GetEmpty());
			const FKnightCombatSnapshot HumanCombat = HumanCombatComponent->GetSnapshot();
			const FString Message = FString::Printf(
				TEXT("%s: bossHP%.1f phase%d life%d stamina%.1f target%s range%.1f action%s "
					"running%d selectable%d stagger%d tree%d ticks%lld selectContext%s selected%s; "
					"humanHP%.1f life%d block%d attack%d Q%d feedback%s."),
				Reason, BossEnemy->GetKnightAttributeSet()->GetHealth(), BossEnemy->GetBossPhaseComponent()->GetPhaseIndex(),
				static_cast<int32>(BossEnemy->GetLifeComponent()->GetLifeState()), Decision.Stamina,
				*GetNameSafe(Decision.Target.Get()), Decision.TargetDistance, *Decision.ActiveActionTag.ToString(),
				Decision.bActionRunning, Decision.bCanSelectAction, Decision.bStaggered,
				AI != nullptr && AI->GetDecisionTree() != nullptr && AI->GetDecisionTree()->IsRunning(),
				AI != nullptr ? AI->GetDecisionTickCount() : int64(-1),
				*Selection.Context.ToString(), *Selected.ActionTag.ToString(), Knight->GetKnightAttributeSet()->GetHealth(),
				static_cast<int32>(Knight->GetLifeComponent()->GetLifeState()), HumanCombat.bBlocking, HumanCombat.bAttacking,
				DownKeys.Contains(EKeys::Q), *Controller->GetLastCommandResult().Message.ToString());
			Test.AddInfo(Message);
			UE_LOG(LogKnightArenaPlaythrough, Display, TEXT("%s"), *Message);
		}

		bool ValidateReset()
		{
			if (State->GetSnapshot().Stage != EKnightArenaStage::AwaitingStart
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
			for (const TWeakObjectPtr<AKnightCharacter>& Entry : Knights)
			{
				AKnightCharacter* Knight = Entry.Get();
				UKnightAbilitySystemComponent* ASC = Knight->GetKnightAbilitySystem();
				if (!Knight->GetLifeComponent()->IsAlive() || Knight->GetLifeComponent()->IsReviveInProgress()
					|| !FMath::IsNearlyEqual(Knight->GetKnightAttributeSet()->GetHealth(), Knight->GetKnightAttributeSet()->GetMaxHealth())
					|| ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Reviving)
					|| ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Attacking)
					|| ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Staggered)
					|| Knight->FindComponentByClass<UKnightCameraModeComponent>()->GetModeCount() != 0)
				{
					Fail(TEXT("Retry did not reset existing canonical GAS life/resources/actions/camera."));
					return false;
				}
				if (AKnightArenaCompanionController* Companion = Cast<AKnightArenaCompanionController>(Knight->GetController()))
				{
					if (!Companion->GetArenaInitializationResult().IsSuccess()
						|| !Companion->GetFollowComponent()->IsRunning()
						|| Companion->GetAIContext()->GetControlledPawn() != Knight
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
			AKnightArenaEnemy* Source = FindLiveEnemy();
			if (Source == nullptr)
			{
				Fail(TEXT("Wipe fixture has no real hostile GAS source."));
				return;
			}
			for (const TWeakObjectPtr<AKnightCharacter>& Knight : Knights)
			{
				if (!UKnightAbilitySystemComponent::ApplyDamage(Source, Knight.Get(), 1000.0f).IsSuccess())
				{
					Fail(TEXT("Explicit GAS wipe fault injection was rejected."));
					return;
				}
			}
			Enter(EKnightArenaTestPhase::Wipe);
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
			if (Phase == EKnightArenaTestPhase::Cleanup || Phase == EKnightArenaTestPhase::Complete)
			{
				return;
			}
			if (State.IsValid())
			{
				const FKnightPartyArenaSnapshot Snapshot = State->GetSnapshot();
				Test.AddInfo(FString::Printf(TEXT("Arena failure phase%d elapsed%.2f stage%d revision%d enemies%d bossphase%d alive%d downed%d leases%d; companionGAS%d hits%d revive%d enemyGAS%d boss0%d boss1%d heavy%d."),
					static_cast<int32>(Phase), WorldElapsed(), static_cast<int32>(Snapshot.Stage), Snapshot.Revision,
					Snapshot.AliveEnemies, Snapshot.BossPhase, Snapshot.AliveKnights, Snapshot.DownedKnights,
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
			if (Phase == EKnightArenaTestPhase::Cleanup || Phase == EKnightArenaTestPhase::Complete)
			{
				return;
			}
			AbortOwnedQuery();
			ReleaseInput();
			Enter(EKnightArenaTestPhase::Cleanup, 15.0);
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
			Enter(EKnightArenaTestPhase::Complete);
		}

		FAutomationTestBase& Test;
		EKnightArenaTestPhase Phase = EKnightArenaTestPhase::Navigation;
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
		int32 CompanionConfirmedHits = 0;
		int32 MaximumLeases = 0;
		int32 BossGrantCount = 0;
		int32 BossSpecCount = 0;
		int32 OwnedQueryId = INDEX_NONE;
		FKnightEntityId BossId;
		FKey PulseKey;
		TSet<FKey> DownKeys;
		TArray<double> FollowDistances;
		TArray<TWeakObjectPtr<AKnightCharacter>> Knights;
		TArray<FKnightEntityId> KnightIds;
		TArray<FPrimaryAssetId> HeroIds;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AKnightPartyArenaGameMode> Mode;
		TWeakObjectPtr<AKnightArenaPlayerController> Controller;
		TWeakObjectPtr<AKnightPartyState> Party;
		TWeakObjectPtr<AKnightPartyArenaState> State;
		TWeakObjectPtr<AKnightPartyArenaHUD> HUD;
		TWeakObjectPtr<UKnightCombatDirectorSubsystem> Director;
		TWeakObjectPtr<AKnightArenaEnemy> QueryEnemy;
		TWeakObjectPtr<AKnightEnemyController> QueryController;
		TWeakObjectPtr<AKnightArenaEnemy> BossEnemy;
		TWeakObjectPtr<UEnvQuery> QueryTemplate;
		FTransform QueryEnemyTransform;
		FVector HumanMovementOrigin = FVector::ZeroVector;
		TSharedPtr<FKnightArenaQueryObservation> QueryObservation;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		TArray<FIntPoint> OriginalPositions;
		FIntPoint OriginalLastSize = FIntPoint::ZeroValue;
		FIntPoint OriginalPosition = FIntPoint::ZeroValue;
		int32 OriginalWidth = 0;
		int32 OriginalHeight = 0;
		FWaitForEditorNavMeshBuildCommand NavigationWait;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightPartyArenaPlaythroughTest, "Knight.AI.Arena.Playthrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightPartyArenaPlaythroughTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->IsPlaySessionInProgress()
		|| !FPackageName::DoesPackageExist(KnightPartyArenaTest::MapPath)
		|| !FEditorFileUtils::LoadMap(KnightPartyArenaTest::MapPath, false, false))
	{
		AddError(TEXT("Run KnightBootstrap -Milestone=4 on the integrated build and use an idle Editor."));
		return false;
	}
	AddInfo(TEXT("M4 uses actual mapped movement/commands/start/hammer/block/retry, shared StateTree/GAS and asynchronous EQS. Placement/down/wipe are explicit bounded negative-case fixture setup."));
	AddCommand(new KnightPartyArenaTest::FKnightArenaScenario(*this));
	return true;
}

#endif
