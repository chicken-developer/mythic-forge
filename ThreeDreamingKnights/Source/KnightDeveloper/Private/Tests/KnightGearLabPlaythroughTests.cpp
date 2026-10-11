#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Attributes/KnightAttributeSet.h"
#include "Application/ThrottleManager.h"
#include "Abilities/GameplayAbility.h"
#include "Blueprint/WidgetTree.h"
#include "Catalog/KnightGearCatalog.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Components/ScrollBox.h"
#include "Editor.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GearLab/KnightGearLabGameMode.h"
#include "GearLab/KnightGearLabPlayerController.h"
#include "GearLab/KnightGearLabState.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Party/KnightPartyState.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "PartyArena/KnightPartyArenaStartMarker.h"
#include "PartyArena/KnightPartyArenaState.h"
#include "PlayInEditorDataTypes.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "RPGLoop/KnightRPGUpgradeStation.h"
#include "RenderTimer.h"
#include "Editor/EditorPerformanceSettings.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "SkillLab/KnightBossArmorComponent.h"
#include "SkillLab/KnightRunestone.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "Tags/KnightGameplayTags.h"
#include "Tags/KnightFixtureGameplayTags.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/KnightRPGActionButton.h"
#include "Widgets/KnightRPGPanel.h"
#include "Widgets/SViewport.h"

namespace KnightGearPlaythrough
{
	const TCHAR* MapPath = TEXT("/Game/Knight/Maps/Playables/L_M05_6_GearLab");

	enum class EPhase : uint8
	{
		Navigation, Start, Ready, WalkStart, Fight, WalkFrost, ChannelFrost, WalkSanctuary,
		OpenInventory, Claim, Gear, Recovery, ClosePanel, Retry, ReturnForSnapshot,
		SnapshotPanel, SnapshotTravel, NewRunPanel, NewRunTravel, Cleanup, Complete
	};

	struct FObservation
	{
		int32 Replies = 0;
		FKnightRPGCommandResult LastReply;
		FGuid LastFixtureRequestId;
		FKnightResult LastFixtureReply;
		int32 HumanHits = 0;
		int32 HumanHeavyHits = 0;
		int32 HumanHeavyCommits = 0;
		FKnightResult LastGameplayReply;
		bool bArmorBroken = false;
		float MinimumArmorDurability = 100.0f;
	};

	struct FUIOperation
	{
		FName CatalogId;
		FString Action;
		int32 RingTarget = 0;
		bool bEquipped = false;
		bool bVendor = false;
	};

	struct FConsoleState
	{
		FString Value;
		EConsoleVariableFlags Priority = ECVF_SetByConstructor;
	};

	/** Victories and rewards follow mapped input and focused native widgets; no damage or inventory injection. */
	class FPlaythroughCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FPlaythroughCommand(FAutomationTestBase& InTest)
			: Test(InTest), Started(FPlatformTime::Seconds()), PhaseStarted(Started),
			Observation(MakeShared<FObservation>())
		{
		}

		virtual ~FPlaythroughCommand() override
		{
			ReleaseInputs();
			Unbind();
			if (bOwnSession && GEditor != nullptr && GEditor->IsPlayingSessionInEditor())
			{
				GEditor->EndPlayMap();
			}
			RestoreSettings();
		}

		virtual bool Update() override
		{
			if (Phase >= EPhase::WalkStart && Phase < EPhase::Cleanup &&
				Phase != EPhase::SnapshotTravel && Phase != EPhase::NewRunTravel && !Controller.IsValid())
			{
				Test.AddError(TEXT("Mapped GearLab unexpectedly lost its PIE controller outside requested travel."));
				BeginCleanup();
			}
			if (Phase < EPhase::Cleanup && (Test.HasAnyErrors() || FPlatformTime::Seconds() - Started > 720.0
				|| Elapsed() > (Phase == EPhase::Fight ? 180.0 : 75.0)))
			{
				if (!Test.HasAnyErrors())
				{
					Test.AddError(FString::Printf(TEXT("Mapped GearLab timeout phase=%d encounter=%d operation=%d step=%d, last reply=%s."),
						static_cast<int32>(Phase), Encounter, OperationIndex, OperationStep,
						*Observation->LastReply.Result.Context.ToString()));
				}
				if (Controller.IsValid() && Controller->GetControlledKnight() != nullptr)
				{
					const AKnightCharacter* Knight = Controller->GetControlledKnight();
					Test.AddInfo(FString::Printf(TEXT("Mapped failure state: Knight %s alive=%d HP=%.2f location=%s moveIgnored=%d W=%d pathPoints=%d waypoint=%d goal=%s."),
						*Knight->GetKnightEntityId().Value.ToString(), Knight->GetLifeComponent()->IsAlive(),
						Knight->GetKnightAttributeSet()->GetHealth(), *Knight->GetActorLocation().ToString(),
						Controller->IsMoveInputIgnored(), HeldKeys.Contains(EKeys::W), WalkPath.Num(), Waypoint,
						*PathGoal.ToString()));
					Capture(TEXT("FAILED_Runtime"));
				}
				BeginCleanup();
			}
			const double PulseDuration = PulseKey == EKeys::A || PulseKey == EKeys::D || PulseKey == EKeys::W
				? 0.35 : 0.10;
			if (Controller.IsValid() && World.IsValid() && PulseKey.IsValid()
				&& World->GetTimeSeconds() - PulseStarted >= PulseDuration)
			{
				SetKey(PulseKey, false);
				PulseKey = FKey();
			}
			switch (Phase)
			{
			case EPhase::Navigation:
				if (NavigationWait.Update())
				{
					Enter(EPhase::Start);
				}
				break;
			case EPhase::Start: Start(); break;
			case EPhase::Ready: Ready(); break;
			case EPhase::WalkStart: WalkStart(); break;
			case EPhase::Fight: Fight(); break;
			case EPhase::WalkFrost: WalkFrost(); break;
			case EPhase::ChannelFrost: ChannelFrost(); break;
			case EPhase::WalkSanctuary: WalkSanctuary(); break;
			case EPhase::OpenInventory: OpenInventory(); break;
			case EPhase::Claim: Claim(); break;
			case EPhase::Gear: Gear(); break;
			case EPhase::Recovery: Recovery(); break;
			case EPhase::ClosePanel: ClosePanel(); break;
			case EPhase::Retry: Retry(); break;
			case EPhase::ReturnForSnapshot: ReturnForSnapshot(); break;
			case EPhase::SnapshotPanel: SnapshotPanel(); break;
			case EPhase::SnapshotTravel: SnapshotTravel(); break;
			case EPhase::NewRunPanel: NewRunPanel(); break;
			case EPhase::NewRunTravel: NewRunTravel(); break;
			case EPhase::Cleanup: Cleanup(); break;
			default: break;
			}
			return Phase == EPhase::Complete;
		}

	private:
		double Elapsed() const
		{
			return FPlatformTime::Seconds() - PhaseStarted;
		}

		void Enter(EPhase Next)
		{
			Phase = Next;
			PhaseStarted = FPlatformTime::Seconds();
			WalkPath.Reset();
			NextPathAt = -1.0;
			PendingButton.Reset();
			Test.AddInfo(FString::Printf(TEXT("Mapped GearLab phase=%d encounter=%d at %.2fs."),
				static_cast<int32>(Next), Encounter, PhaseStarted - Started));
			UE_LOG(LogTemp, Display, TEXT("Mapped GearLab phase=%d encounter=%d at %.2fs."),
				static_cast<int32>(Next), Encounter, PhaseStarted - Started);
		}

		void Start()
		{
			if (GEditor == nullptr || GEngine == nullptr || GEditor->IsPlaySessionInProgress()
				|| !FPackageName::DoesPackageExist(MapPath))
			{
				Test.AddError(TEXT("Mapped GearLab requires idle Editor and the genuine authored entry map."));
				return;
			}
			const ULevelEditorPlaySettings* Defaults = GetDefault<ULevelEditorPlaySettings>();
			Positions = Defaults->MultipleInstancePositions;
			LastSize = Defaults->LastSize;
			WindowPosition = Defaults->NewWindowPosition;
			Width = Defaults->NewWindowWidth;
			Height = Defaults->NewWindowHeight;
			Settings.Reset(DuplicateObject<ULevelEditorPlaySettings>(Defaults, GetTransientPackage()));
			Settings->SetPlayNetMode(PIE_Standalone);
			Settings->SetPlayNumberOfClients(1);
			Settings->SetRunUnderOneProcess(true);
			Settings->bLaunchSeparateServer = false;
			Settings->EnableGameSound = false;
			Settings->GameGetsMouseControl = true;
			Settings->SetClientWindowSize(FIntPoint(1280, 720));
			// Scoped verification setup keeps real animation frames available in background PIE.
			// This does not change simulation time, gameplay settings or persisted user preferences.
			bSavedBackgroundThrottle = GetDefault<UEditorPerformanceSettings>()->bThrottleCPUWhenNotForeground;
			GetMutableDefault<UEditorPerformanceSettings>()->bThrottleCPUWhenNotForeground = false;
			FSlateThrottleManager::Get().DisableThrottle(true);
			bOwnThrottleOverride = true;
			for (const TPair<FString, FString>& Override : TMap<FString, FString>{
				{TEXT("t.MaxFPS"), TEXT("60")}, {TEXT("r.VSync"), TEXT("0")},
				{TEXT("r.VSyncEditor"), TEXT("0")}, {TEXT("t.IdleWhenNotForeground"), TEXT("0")}})
			{
				if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Override.Key))
				{
					const EConsoleVariableFlags Priority =
						static_cast<EConsoleVariableFlags>(Variable->GetFlags() & ECVF_SetByMask);
					SavedConsoleValues.Add(Override.Key, {Variable->GetString(), Priority});
					// Explicit Set also supports constructor-priority variables in UE 5.8.
					Variable->Set(*Override.Value, Priority);
				}
			}
			FRequestPlaySessionParams PlayRequest;
			PlayRequest.EditorPlaySettings = Settings.Get();
			PlayRequest.GlobalMapOverride = MapPath;
			PlayRequest.bAllowOnlineSubsystem = false;
			GEditor->RequestPlaySession(PlayRequest);
			bOwnSession = true;
			Enter(EPhase::Ready);
		}

		bool BindWorld(UWorld* InWorld)
		{
			AKnightGearLabGameMode* FoundMode = InWorld != nullptr
				? InWorld->GetAuthGameMode<AKnightGearLabGameMode>() : nullptr;
			AKnightGearLabPlayerController* FoundPlayer = InWorld != nullptr
				? Cast<AKnightGearLabPlayerController>(InWorld->GetFirstPlayerController()) : nullptr;
			if (FoundMode == nullptr || FoundPlayer == nullptr || !FoundMode->GetRPGInitializationResult().IsSuccess()
				|| FoundMode->GetRPGState() == nullptr || FoundMode->GetGearLabState() == nullptr
				|| FoundPlayer->GetControlledKnight() == nullptr || !InWorld->HasBegunPlay())
			{
				return false;
			}
			ULocalPlayer* Local = FoundPlayer->GetLocalPlayer();
			const TSharedPtr<FSlateUser> User = Local != nullptr ? Local->GetSlateUser() : nullptr;
			if (!FSlateApplication::IsInitialized() || !User.IsValid())
			{
				return false;
			}
			Unbind();
			World = InWorld;
			Mode = FoundMode;
			Controller = FoundPlayer;
			SlateUserIndex = User->GetUserIndex();
			const TWeakPtr<FObservation> Weak(Observation);
			ReplyHandle = Controller->OnRPGCommandResult.AddLambda([Weak](const FKnightRPGCommandResult& Reply)
			{
				if (const TSharedPtr<FObservation> Capture = Weak.Pin())
				{
					++Capture->Replies;
					Capture->LastReply = Reply;
				}
			});
			GameplayReplyHandle = Controller->OnCommandResult.AddLambda([Weak](const FKnightResult& Reply)
			{
				if (const TSharedPtr<FObservation> Capture = Weak.Pin())
				{
					Capture->LastGameplayReply = Reply;
				}
			});
			FixtureReplyHandle = Controller->OnGearFixtureResult.AddLambda(
				[Weak](const FGuid& RequestId, const FKnightResult& Reply)
				{
					if (const TSharedPtr<FObservation> Capture = Weak.Pin())
					{
						Capture->LastFixtureRequestId = RequestId;
						Capture->LastFixtureReply = Reply;
					}
				});
			ObservedASC = Controller->GetControlledKnight()->GetKnightAbilitySystem();
			AbilityCommitHandle = ObservedASC->AbilityCommittedCallbacks.AddLambda([Weak](UGameplayAbility* Ability)
			{
				if (const TSharedPtr<FObservation> Capture = Weak.Pin(); Capture.IsValid() && Ability != nullptr &&
					Ability->GetAssetTags().HasTagExact(KnightGameplayTags::Ability_Combat_Heavy))
				{
					++Capture->HumanHeavyCommits;
				}
			});
			ObservedCombat = Controller->GetControlledKnight()->GetCombatComponent();
			CombatHandle = ObservedCombat->OnCombatOutcome.AddLambda([Weak](const FKnightCombatOutcome& Outcome)
			{
				if (const TSharedPtr<FObservation> Capture = Weak.Pin())
				{
					if ((Outcome.Outcome == EKnightCombatOutcome::LightHit || Outcome.Outcome == EKnightCombatOutcome::HeavyHit)
						&& Outcome.Amount > 0.0f)
					{
						++Capture->HumanHits;
						if (Outcome.Outcome == EKnightCombatOutcome::HeavyHit)
						{
							++Capture->HumanHeavyHits;
						}
					}
				}
			});
			for (TActorIterator<AKnightPartyArenaStartMarker> It(InWorld); It; ++It)
			{
				StartMarker = *It;
				break;
			}
			return StartMarker.IsValid();
		}

		UKnightRPGComponent* RPG() const
		{
			return Mode->GetRPGState()->GetRPG();
		}

		FKnightInventoryView Inventory() const
		{
			return Mode->GetRPGState()->GetInventory()->GetView();
		}

		void Ready()
		{
			UWorld* PIE = GEditor->PlayWorld.Get();
			if (PIE == nullptr || PIE->GetTimeSeconds() < 0.75 || !BindWorld(PIE))
			{
				return;
			}
			WorldContext = GEngine->GetWorldContextFromWorldChecked(PIE).ContextHandle;
			RunId = RPG()->GetView().DemoRunId;
			for (const FKnightRosterEntry& Entry : Mode->GetPartyState()->GetRoster())
			{
				CanonicalIds.Add(Entry.KnightId);
				Test.TestEqual(TEXT("Progression fixture starts level1"), RPG()->GetKnightView(Entry.KnightId).Level, 1);
			}
			Test.TestEqual(TEXT("Progression fixture starts currency30"), RPG()->GetView().Currency, int64(30));
			Test.TestTrue(TEXT("Starter totals come from real equipped sources"),
				FMath::IsNearlyEqual(Controller->GetControlledKnight()->GetKnightAttributeSet()->GetMaxHealth(), 1140.0f));
			Capture(TEXT("01_Starter"));
			Enter(EPhase::WalkStart);
		}

		void SetKey(const FKey& Key, bool bPressed)
		{
			if (!Controller.IsValid())
			{
				return;
			}
			FScopedConditionalWorldSwitcher Switch(Controller->GetWorld());
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key,
				bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f));
			if (bPressed)
			{
				HeldKeys.Add(Key);
			}
			else
			{
				HeldKeys.Remove(Key);
			}
		}

		void Pulse(const FKey& Key)
		{
			if (!PulseKey.IsValid())
			{
				PulseKey = Key;
				PulseStarted = World->GetTimeSeconds();
				SetKey(Key, true);
			}
		}

		void ReleaseInputs()
		{
			for (const FKey& Key : HeldKeys.Array())
			{
				SetKey(Key, false);
			}
			PulseKey = FKey();
		}

		bool Walk(const FVector& Goal, float Reach = 135.0f)
		{
			AKnightCharacter* Knight = Controller->GetControlledKnight();
			if (FVector::DistSquared2D(Knight->GetActorLocation(), Goal) <= FMath::Square(Reach))
			{
				SetKey(EKeys::W, false);
				return true;
			}
			if (!Knight->GetLifeComponent()->IsAlive())
			{
				SetKey(EKeys::W, false);
				return false;
			}
			if (World->GetTimeSeconds() >= NextPathAt || WalkPath.IsEmpty()
				|| FVector::DistSquared2D(PathGoal, Goal) > FMath::Square(100.0f))
			{
				NextPathAt = World->GetTimeSeconds() + 0.4;
				PathGoal = Goal;
				FScopedConditionalWorldSwitcher Switch(World.Get());
				UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
					World.Get(), Knight->GetActorLocation(), Goal, Knight);
				WalkPath = Path != nullptr && Path->IsValid() && !Path->IsPartial() ? Path->PathPoints : TArray<FVector>();
				Waypoint = WalkPath.Num() > 1 ? 1 : 0;
			}
			if (WalkPath.IsEmpty())
			{
				return false;
			}
			while (Waypoint < WalkPath.Num() - 1
				&& FVector::DistSquared2D(Knight->GetActorLocation(), WalkPath[Waypoint]) < FMath::Square(15.0f))
			{
				++Waypoint;
			}
			FVector Direction = WalkPath[Waypoint] - Knight->GetActorLocation();
			Direction.Z = 0.0;
			Controller->SetControlRotation(Direction.Rotation());
			if (!HeldKeys.Contains(EKeys::W))
			{
				SetKey(EKeys::W, true);
			}
			const double Now = World->GetTimeSeconds();
			if (FVector::DistSquared2D(LastWalkLocation, Knight->GetActorLocation()) > FMath::Square(12.0f))
			{
				LastWalkLocation = Knight->GetActorLocation();
				LastWalkProgressAt = Now;
			}
			else if (Now - LastWalkProgressAt > 1.0 && !PulseKey.IsValid())
			{
				// Real lateral input clears capsule/body obstruction; navigation never teleports the human.
				Pulse(bSidestepRight ? EKeys::D : EKeys::A);
				bSidestepRight = !bSidestepRight;
				LastWalkProgressAt = Now;
				Test.AddInfo(FString::Printf(TEXT("Mapped navigation sidestep at %s towards %s."),
					*Knight->GetActorLocation().ToString(), *WalkPath[Waypoint].ToString()));
			}
			return false;
		}

		void WalkStart()
		{
			const EKnightArenaStage Stage = Mode->GetArenaState()->GetSnapshot().Stage;
			if (Stage == EKnightArenaStage::Wave || Stage == EKnightArenaStage::Boss)
			{
				HitsBefore = Observation->HumanHits;
				Enter(Encounter == 2 ? EPhase::WalkFrost : EPhase::Fight);
				return;
			}
			if (!PulseKey.IsValid() && Walk(StartMarker->GetActorLocation()))
			{
				Pulse(EKeys::F);
			}
		}

		AKnightArenaEnemy* NearestEnemy() const
		{
			AKnightArenaEnemy* Nearest = nullptr;
			double Best = TNumericLimits<double>::Max();
			for (AKnightArenaEnemy* Enemy : Mode->GetEncounterEnemies())
			{
				if (IsValid(Enemy) && Enemy->GetLifeComponent()->IsAlive())
				{
					const double Distance = FVector::DistSquared2D(Controller->GetControlledKnight()->GetActorLocation(),
						Enemy->GetActorLocation());
					if (Distance < Best)
					{
						Best = Distance;
						Nearest = Enemy;
					}
				}
			}
			return Nearest;
		}

		void Fight()
		{
			if (Encounter == 2 && !bArmorScreenshotCaptured && ObservedArmor.IsValid()
				&& ObservedArmor->GetArmorState() == EKnightBossArmorState::Broken)
			{
				if (ArmorCaptureStarted < 0.0)
				{
					ArmorCaptureStarted = FPlatformTime::Seconds();
					const FVector ViewDirection = ObservedArmor->GetOwner()->GetActorLocation()
						- Controller->GetControlledKnight()->GetActorLocation();
					Controller->SetControlRotation(ViewDirection.Rotation());
				}
				else if (FPlatformTime::Seconds() - ArmorCaptureStarted >= 0.10)
				{
					Capture(TEXT("07_ArmorBroken"));
					bArmorScreenshotCaptured = true;
				}
			}
			const double RealNow = FPlatformTime::Seconds();
			if (FrameSampleStarted == 0.0)
			{
				FrameSampleStarted = RealNow;
				FrameSampleCounter = GFrameCounter;
			}
			else if (RealNow - FrameSampleStarted >= 2.0)
			{
				MeasuredFrameRate = static_cast<double>(GFrameCounter - FrameSampleCounter)
					/ (RealNow - FrameSampleStarted);
				UE_LOG(LogTemp, Display, TEXT("Mapped real PIE cadence %.2fFPS dt%.4f worldDelta%.4f GT%.2fms RT%.2fms RHI%.2fms GPU%.2fms."),
					MeasuredFrameRate, FApp::GetDeltaTime(), World->GetDeltaSeconds(),
					FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime),
					FPlatformTime::ToMilliseconds(GRHIThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
				FrameSampleStarted = RealNow;
				FrameSampleCounter = GFrameCounter;
			}
			const EKnightArenaStage Stage = Mode->GetArenaState()->GetSnapshot().Stage;
			if (Stage == EKnightArenaStage::Defeat)
			{
				Test.AddError(TEXT("Party wiped during actual mapped GearLab progression."));
				return;
			}
			if (Stage == EKnightArenaStage::Victory)
			{
				ReleaseInputs();
				Test.TestTrue(TEXT("Every encounter includes positive confirmed human hammer damage"),
					Observation->HumanHits > HitsBefore);
				Test.TestTrue(TEXT("Genuine victory exposes one claimable fixture reward"),
					Mode->GetGearLabState()->GetSnapshot().ClaimableRewardId.IsValid());
				Enter(EPhase::WalkSanctuary);
				return;
			}
			AKnightArenaEnemy* Enemy = NearestEnemy();
			AKnightCharacter* Knight = Controller->GetControlledKnight();
			if (Enemy == nullptr || !Knight->GetLifeComponent()->IsAlive())
			{
				SetKey(EKeys::W, false);
				return;
			}
			const double Now = World->GetTimeSeconds();
			if (Encounter == 2 && Now >= NextFightDiagnosticAt)
			{
				NextFightDiagnosticAt = Now + 2.0;
				const FKnightCombatSnapshot& Combat = Knight->GetCombatComponent()->GetSnapshot();
				UE_LOG(LogTemp, Display, TEXT("Mapped C state: distance%.2f Knight%s enemy%s lock%s transfer%d attack%d stagger%d W%d pulse%s heavyInputs%d commits%d hits%d lastError%d/%s."),
					FVector::Dist2D(Knight->GetActorLocation(), Enemy->GetActorLocation()),
					*Knight->GetActorLocation().ToString(), *Enemy->GetActorLocation().ToString(),
					*GetNameSafe(Combat.LockedTarget.Get()), Knight->GetCombatComponent()->CanTransferControl(),
					Combat.bAttacking, Combat.bStaggered, HeldKeys.Contains(EKeys::W), *PulseKey.ToString(),
					HeavyInputCount, Observation->HumanHeavyCommits, Observation->HumanHeavyHits,
					static_cast<int32>(Observation->LastGameplayReply.Error), *Observation->LastGameplayReply.Context.ToString());
			}
			if (PulseKey.IsValid() || !Walk(Enemy->GetActorLocation(), Encounter == 2 ? 105.0f : 130.0f)
				|| !Knight->GetCombatComponent()->CanTransferControl())
			{
				return;
			}
			if (Encounter == 2 && !bBossLockRequested && Knight->GetCombatComponent()->GetLockedTarget() == nullptr)
			{
				// Lock is optional; a rejected LOS query must not suppress all normal attacks.
				bBossLockRequested = true;
				Pulse(EKeys::MiddleMouseButton);
				return;
			}
			if (Encounter == 2 && !Observation->bArmorBroken && Observation->HumanHeavyCommits > 0 &&
				Now - LastContactStepAt >= 3.0)
			{
				LastContactStepAt = Now;
				Pulse(bSidestepRight ? EKeys::D : EKeys::A);
				bSidestepRight = !bSidestepRight;
				return;
			}
			if (Now - LastActionAt < 0.95)
			{
				return;
			}
			UKnightSkillRuntimeComponent* Runtime = Knight->FindComponentByClass<UKnightSkillRuntimeComponent>();
			const FKnightSkillRuntimeState State = Runtime->GetSkillState();
			const UKnightAttributeSet* Attributes = Knight->GetKnightAttributeSet();
			LastActionAt = Now;
			if (Attributes->GetHealth() < Attributes->GetMaxHealth() * 0.70f && State.ConsumableEnd <= Now)
			{
				Pulse(EKeys::One);
			}
			else if (Encounter == 2 && !Observation->bArmorBroken)
			{
				const FVector Facing = (Enemy->GetActorLocation() - Knight->GetActorLocation()).GetSafeNormal2D();
				Controller->SetControlRotation(Facing.Rotation());
				if (FVector::DotProduct(Knight->GetActorForwardVector(), Facing) < 0.98)
				{
					// The character turns through movement, not camera yaw. An actual forward input
					// faces the target after lock loss or a sidestep before the mapped heavy attack.
					Pulse(EKeys::W);
					return;
				}
				++HeavyInputCount;
				Pulse(EKeys::RightMouseButton);
			}
			else if (Observation->HumanHits == HitsBefore)
			{
				Pulse(EKeys::LeftMouseButton);
			}
			else if (ActionCounter++ % 4 == 0 && Runtime->IsSkillAvailable(4))
			{
				Pulse(EKeys::V);
			}
			else if (ActionCounter % 5 == 0 && Runtime->IsSkillAvailable(3))
			{
				Pulse(EKeys::C);
			}
			else if (ActionCounter % 3 == 0 && Runtime->IsSkillAvailable(2))
			{
				Pulse(EKeys::X);
			}
			else
			{
				Pulse(Encounter == 2 ? EKeys::RightMouseButton : EKeys::LeftMouseButton);
			}
		}

		void WalkFrost()
		{
			if (!Frost.IsValid())
			{
				for (TActorIterator<AKnightRunestone> It(World.Get()); It; ++It)
				{
					if (It->GetKind() == EKnightRunestoneKind::Frost)
					{
						Frost = *It;
						break;
					}
				}
			}
			if (Frost.IsValid() && !PulseKey.IsValid() && Walk(Frost->GetActorLocation(), 125.0f))
			{
				Pulse(EKeys::F);
				Enter(EPhase::ChannelFrost);
			}
		}

		void ChannelFrost()
		{
			if (Frost->GetState() == EKnightRunestoneState::Cooldown)
			{
				AKnightArenaEnemy* Enemy = NearestEnemy();
				UKnightBossArmorComponent* Armor = Enemy != nullptr
					? Enemy->FindComponentByClass<UKnightBossArmorComponent>() : nullptr;
				Test.TestTrue(TEXT("World Frost Runestone made the real boss armor brittle"),
					Armor != nullptr && Armor->GetArmorState() == EKnightBossArmorState::Brittle);
				if (Armor != nullptr && !ObservedArmor.IsValid())
				{
					ObservedArmor = Armor;
					const TWeakObjectPtr<UKnightBossArmorComponent> WeakArmor(Armor);
					const TWeakPtr<FObservation> WeakObservation(Observation);
					ArmorHandle = Armor->OnArmorChanged.AddLambda([WeakArmor, WeakObservation]()
					{
						if (const TSharedPtr<FObservation> Capture = WeakObservation.Pin();
							Capture.IsValid() && WeakArmor.IsValid())
						{
							Capture->bArmorBroken |= WeakArmor->GetArmorState() == EKnightBossArmorState::Broken;
							Capture->MinimumArmorDurability = FMath::Min(Capture->MinimumArmorDurability,
								WeakArmor->GetDurability());
							UE_LOG(LogTemp, Display, TEXT("Mapped real boss armor durability=%.2f state=%d humanHeavyHits=%d."),
								WeakArmor->GetDurability(), static_cast<int32>(WeakArmor->GetArmorState()), Capture->HumanHeavyHits);
						}
					});
				}
				Capture(TEXT("03_FrostBoss"));
				Enter(EPhase::Fight);
			}
			else if (Elapsed() > 4.0 && Frost->GetState() == EKnightRunestoneState::Ready && !PulseKey.IsValid())
			{
				Pulse(EKeys::F);
			}
		}

		void WalkSanctuary()
		{
			if (!PulseKey.IsValid() && Walk(Mode->GetUpgradeStation()->GetActorLocation()))
			{
				Pulse(EKeys::I);
				Enter(EPhase::OpenInventory);
			}
		}

		bool Focused() const
		{
			const UKnightRPGPanel* Panel = Controller->GetRPGPanel();
			return Panel != nullptr && Panel->IsActivated()
				&& (Panel->HasUserFocus(Controller.Get()) || Panel->HasUserFocusedDescendants(Controller.Get()))
				&& FSlateApplication::Get().GetUserFocusedWidget(SlateUserIndex).IsValid();
		}

		bool Button(const FString& Caption, const FString& Contains = FString(), bool bPreferPageButton = false)
		{
			UKnightRPGPanel* Panel = Controller->GetRPGPanel();
			if (Panel == nullptr || !Panel->IsActivated() || Panel->WidgetTree == nullptr)
			{
				return false;
			}
			TArray<UWidget*> Widgets;
			Panel->WidgetTree->GetAllWidgets(Widgets);
			UKnightRPGActionButton* Match = nullptr;
			for (UWidget* Widget : Widgets)
			{
				UKnightRPGActionButton* Candidate = Cast<UKnightRPGActionButton>(Widget);
				if (Candidate != nullptr && Candidate->IsVisible() && Candidate->GetIsEnabled())
				{
					const FString Text = Candidate->GetCaption().ToString();
					if (Contains.IsEmpty() ? Text == Caption : Text.Contains(Caption) && Text.Contains(Contains))
					{
						Match = Candidate;
						if (!bPreferPageButton)
						{
							break;
						}
					}
				}
			}
			if (Match == nullptr)
			{
				return false;
			}
			if (PendingButton.Get() != Match)
			{
				PendingButton = Match;
				ButtonFocusRequestedAt = FPlatformTime::Seconds();
				ButtonFocusAttempts = 0;
				UWidget* ScrollTarget = Match;
				for (UWidget* Parent = Match->GetParent(); Parent != nullptr; Parent = Parent->GetParent())
				{
					if (UScrollBox* Scroll = Cast<UScrollBox>(Parent))
					{
						Scroll->ScrollWidgetIntoView(ScrollTarget, false, EDescendantScrollDestination::Center);
						ScrollTarget = Scroll;
					}
				}
				Match->SetUserFocus(Controller.Get());
				return false;
			}
			// Confirmed commands replace the tree and defer its default focus until after layout.
			// Wait for that paint, then move focus as a user would rather than awaiting a retired path.
			if (FPlatformTime::Seconds() - ButtonFocusRequestedAt < 0.08)
			{
				return false;
			}
			if (!Match->HasUserFocus(Controller.Get()) && !Match->HasUserFocusedDescendants(Controller.Get()))
			{
				if (++ButtonFocusAttempts > 5)
				{
					Test.AddError(FString::Printf(TEXT("Mapped native button '%s' repeatedly lost local Slate focus after layout."),
						*Match->GetCaption().ToString()));
					return false;
				}
				Match->SetUserFocus(Controller.Get());
				ButtonFocusRequestedAt = FPlatformTime::Seconds();
				return false;
			}
			PendingButton.Reset();
			const FString ActivatedCaption = Match->GetCaption().ToString();
			const FKeyEvent Event(EKeys::Enter, FModifierKeysState(), SlateUserIndex, false, 0, 0);
			FScopedConditionalWorldSwitcher Switch(World.Get());
			const bool bHandled = FSlateApplication::Get().ProcessKeyDownEvent(Event);
			FSlateApplication::Get().ProcessKeyUpEvent(Event);
			Test.AddInfo(FString::Printf(TEXT("Mapped real Enter '%s': handled=%d, focus retries=%d."),
				*ActivatedCaption, bHandled, ButtonFocusAttempts));
			UE_LOG(LogTemp, Display, TEXT("Mapped real Enter '%s': handled=%d, focus retries=%d."),
				*ActivatedCaption, bHandled, ButtonFocusAttempts);
			Test.TestTrue(TEXT("Visible native button handled focused Slate Enter"), bHandled);
			return bHandled;
		}

		void OpenInventory()
		{
			if (Focused() && Button(TEXT("Inventory")))
			{
				ReplyCountBefore = Observation->Replies;
				Enter(EPhase::Claim);
			}
		}

		void Claim()
		{
			if (Observation->Replies > ReplyCountBefore)
			{
				if (!Test.TestTrue(TEXT("Complete encounter reward committed via actual UI"), Observation->LastReply.Result.IsSuccess()))
				{
					return;
				}
				const int32 ExpectedLevels[]{3, 5, 8};
				for (const FKnightEntityId& Id : CanonicalIds)
				{
					Test.TestEqual(TEXT("Reward atomically advances each fixture Knight level"),
						RPG()->GetKnightView(Id).Level, ExpectedLevels[Encounter]);
				}
				Test.TestEqual(TEXT("One claimed reward per completed encounter"), RPG()->GetView().ClaimedRewards.Num(), Encounter + 1);
				BuildOperations();
				Enter(EPhase::Gear);
				return;
			}
			Button(TEXT("Claim confirmed encounter reward"));
		}

		void BuildOperations()
		{
			Operations.Reset();
			OperationIndex = 0;
			OperationStep = 0;
			const FString Equip(TEXT("Stage equip to selected slot"));
			if (Encounter == 0)
			{
				Operations.Add({TEXT("N01"), Equip});
				Operations.Add({TEXT("F01"), Equip, 1});
				Operations.Add({TEXT("F02"), Equip, 2});
			}
			else if (Encounter == 1)
			{
				for (const FName Id : {FName(TEXT("W02")), FName(TEXT("A08")), FName(TEXT("A09")),
					FName(TEXT("A10")), FName(TEXT("A11")), FName(TEXT("A12")), FName(TEXT("A13")),
					FName(TEXT("A14")), FName(TEXT("N02")), FName(TEXT("T01"))})
				{
					Operations.Add({Id, Equip});
				}
				Operations.Add({TEXT("F04"), Equip, 2});
				Operations.Add({TEXT("R01"), TEXT("Stage attach Rune to this Golem's weapon")});
				Operations.Add({TEXT("W02"), TEXT("Stage upgrade +1 · 20 currency +4 M01 +2 M02 +1 M03"), 0, true});
				Operations.Add({TEXT("C01"), TEXT("Stage buy one"), 0, false, true});
			}
			else
			{
				Operations.Add({TEXT("W05"), Equip});
			}
		}

		void Gear()
		{
			if (OperationIndex >= Operations.Num())
			{
				const UKnightAttributeSet* Attributes = Controller->GetControlledKnight()->GetKnightAttributeSet();
				if (Encounter == 1)
				{
					Test.TestTrue(TEXT("Twelve equipped Guard slots + upgrade give H1380/P130/A200"),
						RPG()->GetKnightView(Controller->GetControlledKnight()->GetKnightEntityId()).Assignments.Num() == 12
						&& FMath::IsNearlyEqual(Attributes->GetMaxHealth(), 1380.0f)
						&& FMath::IsNearlyEqual(Attributes->GetAttackPower(), 130.0f)
						&& FMath::IsNearlyEqual(Attributes->GetDefense(), 200.0f));
					Test.TestEqual(TEXT("A/B rewards minus upgrade20 and potion purchase3"), RPG()->GetView().Currency, int64(77));
					Capture(TEXT("02_GuardUpgrade"));
				}
				else if (Encounter == 2)
				{
					Test.TestTrue(TEXT("Epic weapon uses original formulas: P136/A207, no double damage source"),
						FMath::IsNearlyEqual(Attributes->GetAttackPower(), 136.0f)
						&& FMath::IsNearlyEqual(Attributes->GetDefense(), 207.0f));
					Test.TestEqual(TEXT("Final fixture currency127"), RPG()->GetView().Currency, int64(127));
					Test.AddInfo(FString::Printf(TEXT("Mapped boss armor minimum durability=%.2f, observed Broken=%d, humanHeavyHits=%d."),
						Observation->MinimumArmorDurability, Observation->bArmorBroken, Observation->HumanHeavyHits));
					Capture(TEXT("04_EpicLoot"));
				}
				Enter(EPhase::Recovery);
				return;
			}
			const FUIOperation& Operation = Operations[OperationIndex];
			const bool bRune = Operation.CatalogId.ToString().StartsWith(TEXT("R"));
			const TCHAR* Category = bRune ? TEXT("Runes") : Operation.bVendor ? TEXT("Consumables") : TEXT("Equipment");
			if (OperationStep == 0)
			{
				if (Controller->GetRPGPanel()->GetActiveTab() != EKnightRPGPanelTab::Inventory)
				{
					Button(TEXT("Inventory"));
					return;
				}
				// The page category and the top-level Attach Runes tab deliberately share a label.
				if (Button(Category, FString(), true))
				{
					++OperationStep;
				}
				return;
			}
			if (OperationStep == 1)
			{
				const FKnightGearSeed* Seed = FKnightGearCatalog::GetSeeds().FindByPredicate([&Operation](const FKnightGearSeed& Value)
				{
					return Value.CatalogId == Operation.CatalogId;
				});
				if (Seed == nullptr)
				{
					Test.AddError(TEXT("Playthrough requested an absent catalog seed."));
					return;
				}
				const FString Contains = Operation.bVendor ? TEXT("currency each")
					: Operation.bEquipped ? TEXT("Location: Equipment") : TEXT("Shared inventory");
				if (Button(Seed->Name.ToString(), Contains))
				{
					++OperationStep;
				}
				return;
			}
			if (OperationStep == 2)
			{
				if (Operation.RingTarget == 0 || Button(FString::Printf(TEXT("Compare Ring%d"), Operation.RingTarget)))
				{
					++OperationStep;
				}
				return;
			}
			if (OperationStep == 3)
			{
				if (Button(Operation.Action))
				{
					++OperationStep;
				}
				return;
			}
			if (OperationStep == 4)
			{
				ReplyCountBefore = Observation->Replies;
				if (Button(TEXT("Apply changes [F]")))
				{
					++OperationStep;
				}
				return;
			}
			if (Observation->Replies > ReplyCountBefore && !Controller->GetRPGPanel()->HasPendingCommand())
			{
				Test.TestTrue(FString::Printf(TEXT("Actual UI %s: %s"), *Operation.CatalogId.ToString(), *Operation.Action),
					Observation->LastReply.Result.IsSuccess());
				++OperationIndex;
				OperationStep = 0;
				PhaseStarted = FPlatformTime::Seconds();
			}
		}

		void Recovery()
		{
			if (Button(TEXT("Fixture: Sanctuary Recovery")))
			{
				Enter(EPhase::ClosePanel);
			}
		}

		void ClosePanel()
		{
			UKnightRPGPanel* Panel = Controller->GetRPGPanel();
			if (Panel->IsActivated())
			{
				// A confirmed fixture reply can still be rebuilding focus. Use the actual native
				// Close control so an unfocused Editor Escape cannot terminate this PIE session.
				Button(TEXT("Close [Esc / B]"));
				return;
			}
			Test.TestFalse(TEXT("CommonUI close restores mapped movement"), Controller->IsMoveInputIgnored());
			++Encounter;
			if (Encounter < 3)
			{
				Enter(EPhase::WalkStart);
			}
			else
			{
				SavedInventory = Inventory();
				SavedView = RPG()->GetView();
				Pulse(EKeys::R);
				Enter(EPhase::Retry);
			}
		}

		void Retry()
		{
			if (Mode->GetArenaState()->GetSnapshot().Stage == EKnightArenaStage::AwaitingStart && !PulseKey.IsValid())
			{
				Test.TestTrue(TEXT("Mapped R keeps currency, exact inventory GUIDs, levels, assignments and reward receipts"),
					Inventory() == SavedInventory && RPG()->GetView() == SavedView);
				Enter(EPhase::ReturnForSnapshot);
			}
		}

		void ReturnForSnapshot()
		{
			if (Walk(Mode->GetUpgradeStation()->GetActorLocation()) && !PulseKey.IsValid())
			{
				Pulse(EKeys::I);
				Enter(EPhase::SnapshotPanel);
			}
		}

		void SnapshotPanel()
		{
			if (!Focused())
			{
				return;
			}
			// Existing strict capture policy rejects active HoT/shield/projectile streams; wait for genuine idle state.
			FKnightRPGSnapshot Captured;
			if (!RPG()->CaptureSnapshot(Captured).IsSuccess())
			{
				return;
			}
			SavedInventory = Inventory();
			SavedView = RPG()->GetView();
			OldWorld = World;
			if (Button(TEXT("Fixture: reconstruct this run from in-memory snapshot")))
			{
				CheckTravelFence(TEXT("SnapshotReconstruct"), &Captured);
				ReleaseInputs();
				Enter(EPhase::SnapshotTravel);
			}
		}

		UWorld* CurrentContext() const
		{
			const FWorldContext* Context = GEngine->GetWorldContextFromHandle(WorldContext);
			return Context != nullptr ? Context->World() : nullptr;
		}

		void SnapshotTravel()
		{
			UWorld* Reloaded = CurrentContext();
			if (Reloaded == nullptr || Reloaded == OldWorld.Get() || Reloaded->GetTimeSeconds() < 0.75 || !BindWorld(Reloaded))
			{
				return;
			}
			Test.TestTrue(TEXT("Fresh world reconstructed exact shared inventory and confirmed ledger"),
				Inventory() == SavedInventory && RPG()->GetView() == SavedView);
			for (const FKnightRosterEntry& Entry : Mode->GetPartyState()->GetRoster())
			{
				Test.TestTrue(TEXT("Reconstruction retains canonical Knight identity"), CanonicalIds.Contains(Entry.KnightId));
				Test.TestFalse(TEXT("A fresh reconstructed ASC retains no old-world travel activation guard"),
					Entry.Knight->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Fixture_TravelPending));
			}
			Capture(TEXT("05_Reconstructed"));
			Enter(EPhase::NewRunPanel);
		}

		void NewRunPanel()
		{
			if (!Focused())
			{
				if (Walk(Mode->GetUpgradeStation()->GetActorLocation()) && !PulseKey.IsValid())
				{
					Pulse(EKeys::I);
				}
				return;
			}
			OldWorld = World;
			if (Button(TEXT("Fixture: New Demo Run")))
			{
				CheckTravelFence(TEXT("NewDemoRun"));
				ReleaseInputs();
				Enter(EPhase::NewRunTravel);
			}
		}

		void CheckTravelFence(const FName Action, const FKnightRPGSnapshot* Captured = nullptr)
		{
			const FKnightInventoryView BeforeInventory = Inventory();
			const FKnightRPGView BeforeView = RPG()->GetView();
			Test.TestTrue(TEXT("Actual fixture UI received a successful authoritative travel receipt"),
				Observation->LastFixtureRequestId.IsValid() && Observation->LastFixtureReply.IsSuccess());
			Test.TestTrue(TEXT("Exact fixture replay returns success while the original travel is pending"),
				Mode->ExecuteFixtureAction(Controller.Get(), Action, Observation->LastFixtureRequestId).IsSuccess());
			const FKnightResult CompetingTravel = Mode->ExecuteFixtureAction(Controller.Get(),
				Action == TEXT("NewDemoRun") ? FName(TEXT("SnapshotReconstruct")) : FName(TEXT("NewDemoRun")),
				FGuid::NewGuid());
			Test.TestTrue(TEXT("A second fresh travel cannot replace an accepted reconstruction or new run"),
				CompetingTravel.Error == EKnightError::InvalidState
				&& CompetingTravel.Context == TEXT("GearFixtureTravelPending"));
			FKnightRPGCommand FreshGear;
			FreshGear.Operation = EKnightRPGOperation::BindConsumable;
			FreshGear.RequestId = FGuid::NewGuid();
			FreshGear.ExpectedKnightId = Controller->GetControlledKnight()->GetKnightEntityId();
			FreshGear.InventoryId = BeforeInventory.InventoryId;
			FreshGear.ExpectedInventoryRevision = BeforeInventory.Revision;
			FreshGear.ExpectedKnightRevision = RPG()->GetKnightView(FreshGear.ExpectedKnightId).Revision;
			FreshGear.ItemDefinitionId = FKnightGearCatalog::DefinitionId(TEXT("C02"));
			FreshGear.QuickSlot = 0;
			const FKnightRPGCommandResult RejectedGear = Mode->ExecuteRPGCommand(Controller.Get(), FreshGear);
			Test.TestTrue(TEXT("Fresh RPG mutations cannot change the ledger after a travel snapshot was accepted"),
				RejectedGear.Result.Error == EKnightError::InvalidState
				&& RejectedGear.Result.Context == TEXT("GearFixtureTravelPending"));
			Test.TestTrue(TEXT("Retry cannot reset transients after an accepted fixture travel"),
				Mode->TryRestartDrill(Controller.Get()).Context == TEXT("GearFixtureTravelPending"));
			Test.TestTrue(TEXT("Interaction cannot start another encounter after an accepted fixture travel"),
				Mode->TryInteract(Controller.Get()).Context == TEXT("GearFixtureTravelPending"));
			if (Action == TEXT("SnapshotReconstruct"))
			{
				for (const FKnightRosterEntry& Entry : Mode->GetPartyState()->GetRoster())
				{
					UKnightAbilitySystemComponent* ASC = Entry.Knight->GetKnightAbilitySystem();
					UKnightSkillRuntimeComponent* Runtime = Entry.Knight->FindComponentByClass<UKnightSkillRuntimeComponent>();
					Test.TestTrue(TEXT("Every canonical ASC owns a snapshot travel activation guard"),
						ASC->HasMatchingGameplayTag(KnightGameplayTags::State_Fixture_TravelPending));
					for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
					{
						Test.TestFalse(TEXT("Native GAS tag requirements reject every old-world granted ability during reconstruction"),
							Spec.Ability->DoesAbilitySatisfyTagRequirements(*ASC));
					}
					const FKnightSkillRuntimeState BeforeSkills = Runtime->GetSkillState();
					const float BeforeResource = Entry.Knight->GetKnightAttributeSet()->GetRuneEnergy();
					Test.TestFalse(TEXT("An actual old-world A4 request cannot create a cooldown-reset gap after capture"),
						Runtime->RequestSkill(4, nullptr).IsSuccess());
					const FKnightSkillRuntimeState AfterSkills = Runtime->GetSkillState();
					Test.TestTrue(TEXT("Rejected post-capture abilities preserve cooldowns, buff and resource exactly"),
						AfterSkills.CooldownEnds == BeforeSkills.CooldownEnds && AfterSkills.BuffEnd == BeforeSkills.BuffEnd
						&& Entry.Knight->GetKnightAttributeSet()->GetRuneEnergy() == BeforeResource);
				}
			}
			if (Captured != nullptr && !Captured->Receipts.IsEmpty())
			{
				const FKnightRPGCommandResult Replay = Mode->ExecuteRPGCommand(Controller.Get(),
					Captured->Receipts.Last().Command);
				Test.TestTrue(TEXT("Authenticated exact RPG receipt remains replayable through the travel fence"),
					Replay.Result.IsSuccess() && Replay.bWasReplay);
			}
			Test.TestTrue(TEXT("Travel races and receipt replay preserve exact confirmed ledger and inventory"),
				Inventory() == BeforeInventory && RPG()->GetView() == BeforeView);
		}

		void NewRunTravel()
		{
			UWorld* Reloaded = CurrentContext();
			if (Reloaded == nullptr || Reloaded == OldWorld.Get() || Reloaded->GetTimeSeconds() < 0.75 || !BindWorld(Reloaded))
			{
				return;
			}
			Test.TestTrue(TEXT("Explicit New Demo Run creates an isolated new run, not repeatable old rewards"),
				RPG()->GetView().DemoRunId != RunId && RPG()->GetView().Currency == 30
				&& RPG()->GetView().ClaimedRewards.IsEmpty() && Mode->GetGearLabState()->GetSnapshot().CompletedEncounters == 0);
			Capture(TEXT("06_NewDemoRun"));
			// Check the latched combat evidence after independent retry/travel checks so one failed
			// armor interaction still provides real coverage of the remaining reconstruction flow.
			Test.TestTrue(TEXT("Boss brittle armor was broken through actual mapped attacks"), Observation->bArmorBroken);
			Test.TestTrue(TEXT("Broken armor was captured from actual rendered gameplay"), bArmorScreenshotCaptured);
			Test.AddInfo(FString::Printf(TEXT("Actual mapped progression A/B/C, Lv1/3/5/8, jewelry, twelve Rare slots, exact-parent Rune, +1 upgrade, vendor, Frost activation, Epic, retry, memory reconstruction and explicit new run reached in %.2fs; armorBroken=%d. This duration is automation evidence; manual10-20minute acceptance remains separate."),
				FPlatformTime::Seconds() - Started, Observation->bArmorBroken));
			BeginCleanup();
		}

		void Capture(const TCHAR* Name)
		{
			ULocalPlayer* Local = Controller->GetLocalPlayer();
			UGameViewportClient* Viewport = Local != nullptr ? Local->ViewportClient.Get() : nullptr;
			const TSharedPtr<SViewport> Widget = Viewport != nullptr ? Viewport->GetGameViewportWidget() : nullptr;
			TArray<FColor> Pixels;
			FIntVector Size(0, 0, 0);
			if (!Widget.IsValid() || !FSlateApplication::Get().TakeScreenshot(Widget.ToSharedRef(), Pixels, Size)
				|| Pixels.Num() < Size.X * Size.Y || Size.X < 640)
			{
				Test.AddError(TEXT("Mapped playthrough could not capture actual Slate/RHI viewport pixels."));
				return;
			}
			Pixels.SetNum(Size.X * Size.Y);
			for (FColor& Pixel : Pixels)
			{
				Pixel.A = 255;
			}
			TArray64<uint8> PNG;
			FImageUtils::PNGCompressImageArray(Size.X, Size.Y,
				TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), PNG);
			const FString Directory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Evidence/M56/playthrough"));
			IFileManager::Get().MakeDirectory(*Directory, true);
			const FString Path = FPaths::Combine(Directory, FString(Name) + TEXT(".png"));
			Test.TestTrue(TEXT("Actual progression PNG saved"), !PNG.IsEmpty() && FFileHelper::SaveArrayToFile(PNG, *Path));
			Test.AddInfo(FString::Printf(TEXT("Rendered GearLab %dx%d: %s"), Size.X, Size.Y, *Path));
		}

		void Unbind()
		{
			if (Controller.IsValid())
			{
				Controller->OnRPGCommandResult.Remove(ReplyHandle);
				Controller->OnCommandResult.Remove(GameplayReplyHandle);
				Controller->OnGearFixtureResult.Remove(FixtureReplyHandle);
			}
			if (ObservedASC.IsValid())
			{
				ObservedASC->AbilityCommittedCallbacks.Remove(AbilityCommitHandle);
			}
			if (ObservedCombat.IsValid())
			{
				ObservedCombat->OnCombatOutcome.Remove(CombatHandle);
			}
			if (ObservedArmor.IsValid())
			{
				ObservedArmor->OnArmorChanged.Remove(ArmorHandle);
				ObservedArmor.Reset();
			}
		}

		void BeginCleanup()
		{
			if (Phase >= EPhase::Cleanup)
			{
				return;
			}
			ReleaseInputs();
			Unbind();
			if (bOwnSession && GEditor != nullptr)
			{
				GEditor->RequestEndPlayMap();
			}
			Enter(EPhase::Cleanup);
		}

		void RestoreSettings()
		{
			if (bOwnThrottleOverride)
			{
				FSlateThrottleManager::Get().DisableThrottle(false);
				GetMutableDefault<UEditorPerformanceSettings>()->bThrottleCPUWhenNotForeground = bSavedBackgroundThrottle;
				bOwnThrottleOverride = false;
				for (const TPair<FString, FConsoleState>& Saved : SavedConsoleValues)
				{
					if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Saved.Key))
					{
						Variable->Set(*Saved.Value.Value, Saved.Value.Priority);
						Test.TestEqual(TEXT("Scoped cadence override restores the original CVar value"),
							Variable->GetString(), Saved.Value.Value);
						Test.TestEqual(TEXT("Scoped cadence override restores the original CVar priority"),
							Variable->GetFlags() & ECVF_SetByMask, static_cast<uint32>(Saved.Value.Priority));
					}
				}
				SavedConsoleValues.Reset();
			}
			if (Settings.IsValid())
			{
				ULevelEditorPlaySettings* Defaults = GetMutableDefault<ULevelEditorPlaySettings>();
				Defaults->MultipleInstancePositions = Positions;
				Defaults->LastSize = LastSize;
				Defaults->NewWindowPosition = WindowPosition;
				Defaults->NewWindowWidth = Width;
				Defaults->NewWindowHeight = Height;
				Defaults->SaveConfig();
			}
		}

		void Cleanup()
		{
			if (GEditor->IsPlaySessionInProgress())
			{
				if (Elapsed() > 15.0 && GEditor->IsPlayingSessionInEditor())
				{
					GEditor->EndPlayMap();
				}
				return;
			}
			RestoreSettings();
			Settings.Reset();
			bOwnSession = false;
			Enter(EPhase::Complete);
		}

		FAutomationTestBase& Test;
		double Started = 0.0;
		double PhaseStarted = 0.0;
		TSharedPtr<FObservation> Observation;
		EPhase Phase = EPhase::Navigation;
		FWaitForEditorNavMeshBuildCommand NavigationWait;
		TStrongObjectPtr<ULevelEditorPlaySettings> Settings;
		bool bOwnSession = false;
		bool bOwnThrottleOverride = false;
		bool bSavedBackgroundThrottle = false;
		TMap<FString, FConsoleState> SavedConsoleValues;
		bool bArmorScreenshotCaptured = false;
		double ArmorCaptureStarted = -1.0;
		double FrameSampleStarted = 0.0;
		uint64 FrameSampleCounter = 0;
		double MeasuredFrameRate = 0.0;
		TArray<FIntPoint> Positions;
		FIntPoint LastSize;
		FIntPoint WindowPosition;
		int32 Width = 0;
		int32 Height = 0;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<UWorld> OldWorld;
		FName WorldContext;
		TWeakObjectPtr<AKnightGearLabGameMode> Mode;
		TWeakObjectPtr<AKnightGearLabPlayerController> Controller;
		TWeakObjectPtr<UKnightCombatComponent> ObservedCombat;
		TWeakObjectPtr<UKnightAbilitySystemComponent> ObservedASC;
		TWeakObjectPtr<UKnightBossArmorComponent> ObservedArmor;
		FDelegateHandle ReplyHandle;
		FDelegateHandle GameplayReplyHandle;
		FDelegateHandle FixtureReplyHandle;
		FDelegateHandle AbilityCommitHandle;
		FDelegateHandle CombatHandle;
		FDelegateHandle ArmorHandle;
		TWeakObjectPtr<AKnightPartyArenaStartMarker> StartMarker;
		TWeakObjectPtr<AKnightRunestone> Frost;
		TWeakObjectPtr<UKnightRPGActionButton> PendingButton;
		double ButtonFocusRequestedAt = 0.0;
		int32 ButtonFocusAttempts = 0;
		TArray<FKnightEntityId> CanonicalIds;
		FGuid RunId;
		int32 SlateUserIndex = 0;
		int32 Encounter = 0;
		int32 HitsBefore = 0;
		int32 ActionCounter = 0;
		double LastActionAt = -10.0;
		double NextFightDiagnosticAt = 0.0;
		double LastContactStepAt = 0.0;
		int32 HeavyInputCount = 0;
		bool bBossLockRequested = false;
		TSet<FKey> HeldKeys;
		FKey PulseKey;
		double PulseStarted = 0.0;
		TArray<FVector> WalkPath;
		FVector PathGoal;
		int32 Waypoint = 0;
		double NextPathAt = -1.0;
		FVector LastWalkLocation = FVector::ZeroVector;
		double LastWalkProgressAt = 0.0;
		bool bSidestepRight = true;
		TArray<FUIOperation> Operations;
		int32 OperationIndex = 0;
		int32 OperationStep = 0;
		int32 ReplyCountBefore = 0;
		FKnightInventoryView SavedInventory;
		FKnightRPGView SavedView;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightGearMappedPlaythroughTest, "Knight.M56.Progression.MappedPlaythrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FKnightGearMappedPlaythroughTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(KnightGearPlaythrough::FPlaythroughCommand(*this));
	return true;
}

#endif
