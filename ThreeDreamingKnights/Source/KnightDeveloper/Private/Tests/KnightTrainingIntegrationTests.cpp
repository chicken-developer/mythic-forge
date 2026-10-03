#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/KnightCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Controllers/KnightCompanionController.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "FileHelpers.h"
#include "Framework/KnightGameState.h"
#include "Framework/KnightPartyGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformTime.h"
#include "Input/KnightInputConfig.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Math/RotationMatrix.h"
#include "Misc/PackageName.h"
#include "Party/KnightPartyState.h"
#include "Player/KnightPlayerController.h"
#include "Settings/KnightSettings.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "Training/KnightTrainingHUD.h"
#include "Training/KnightTrainingMarker.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace KnightTrainingIntegration
{
	const FString MapPath = TEXT("/Game/Knight/Maps/Playables/L_M01_TrainingYard");
	constexpr double TotalTimeoutSeconds = 180.0;
	constexpr double StartupTimeoutSeconds = 30.0;
	constexpr double WalkingTimeoutSeconds = 30.0;
	constexpr double CommandTimeoutSeconds = 5.0;
	constexpr double CohesionTimeoutSeconds = 12.0;
	constexpr double CleanupTimeoutSeconds = 10.0;
	constexpr double MovementProbeSeconds = 0.7;
	constexpr double InputSettleSeconds = 0.3;
	constexpr double MinimumMotionCm = 25.0;
	constexpr double FormationMarginCm = 60.0;
	constexpr double RetryDriftToleranceCm = 25.0;

	enum class EKnightTrainingPhase : uint8
	{
		WaitNavigation,
		StartPIE,
		WaitReady,
		MoveProbe,
		MoveSettle,
		LookProbe,
		JumpProbe,
		WaitLanding,
		WalkMarker,
		WaitCohesion,
		WaitSwitch,
		WaitRetry,
		StopPIE,
		WaitEndPIE,
		Complete
	};

	enum class EKnightTrainingSwitchPurpose : uint8
	{
		NextMarker,
		SelectFirst,
		CycleNext
	};

	/** Drives the real input/movement/overlap path. It never writes training state or teleports a Knight. */
	class FKnightTrainingPlaythroughCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FKnightTrainingPlaythroughCommand(FAutomationTestBase* InTest)
			: Test(InTest), RunStarted(FPlatformTime::Seconds()), PhaseStarted(RunStarted)
		{
		}

		virtual ~FKnightTrainingPlaythroughCommand() override
		{
			ReleaseInput();
			Unsubscribe();
			StartCommand.Reset();
			if (GEditor != nullptr && GEditor->PlayWorld != nullptr)
			{
				GEditor->RequestEndPlayMap();
			}
		}

		virtual bool Update() override
		{
			if (Phase == EKnightTrainingPhase::Complete)
			{
				return true;
			}
			const double Now = FPlatformTime::Seconds();
			if (!bFailed && Now - RunStarted > TotalTimeoutSeconds)
			{
				Fail(TEXT("Training integration exceeded its 180-second wall-clock budget."));
			}
			if (!bFailed && Test->HasAnyErrors())
			{
				Fail(TEXT("Unreal reported an error during the real training playthrough; ending PIE."));
			}
			if (Phase != EKnightTrainingPhase::StopPIE && Phase != EKnightTrainingPhase::WaitEndPIE && Now - PhaseStarted > PhaseTimeout)
			{
				Fail(FString::Printf(TEXT("Timed out in training phase %d, session %d, marker %d."),
					static_cast<int32>(Phase), SessionIndex + 1, MarkerIndex + 1));
			}
			if (Phase >= EKnightTrainingPhase::MoveProbe && Phase <= EKnightTrainingPhase::WaitRetry
				&& (!World.IsValid() || !Controller.IsValid() || !GameState.IsValid() || !Party.IsValid()
					|| Controller->GetControlledKnight() == nullptr))
			{
				Fail(TEXT("The active training world, canonical party, or controlled Knight disappeared during play."));
			}
			if (Phase >= EKnightTrainingPhase::MoveProbe && Phase <= EKnightTrainingPhase::WaitCohesion)
			{
				TrackPhysicalMotion();
			}

			switch (Phase)
			{
			case EKnightTrainingPhase::WaitNavigation:
				if (NavigationWait.Update())
				{
					Test->AddInfo(TEXT("Training map loaded; editor navigation build finished before PIE duplication."));
					EnterPhase(EKnightTrainingPhase::StartPIE, StartupTimeoutSeconds);
				}
				break;
			case EKnightTrainingPhase::StartPIE:
				StartSession();
				break;
			case EKnightTrainingPhase::WaitReady:
				WaitForReady();
				break;
			case EKnightTrainingPhase::MoveProbe:
				if (WorldElapsed() >= MovementProbeSeconds)
				{
					const double Distance = FVector::Dist2D(Controller->GetControlledKnight()->GetActorLocation(), ProbeOrigin);
					if (Distance < MinimumMotionCm)
					{
						Fail(FString::Printf(TEXT("Mapped W input did not move the controlled Knight: %.2f cm."), Distance));
						break;
					}
					Test->AddInfo(FString::Printf(TEXT("PIE session %d: mapped W physically moved CharacterMovement %.2f cm."),
						SessionIndex + 1, Distance));
					ReleaseInput();
					EnterPhase(EKnightTrainingPhase::MoveSettle, CommandTimeoutSeconds);
				}
				break;
			case EKnightTrainingPhase::MoveSettle:
				if (WorldElapsed() >= InputSettleSeconds)
				{
					ProbeYaw = Controller->GetControlRotation().Yaw;
					ProbePitch = Controller->GetControlRotation().Pitch;
					EnterPhase(EKnightTrainingPhase::LookProbe, CommandTimeoutSeconds);
				}
				break;
			case EKnightTrainingPhase::LookProbe:
				InjectAxis(EKeys::MouseX, 1.0f);
				InjectAxis(EKeys::MouseY, 1.0f);
				if (WorldElapsed() >= InputSettleSeconds)
				{
					const double YawChange = FMath::Abs(FMath::FindDeltaAngleDegrees(
						ProbeYaw, static_cast<double>(Controller->GetControlRotation().Yaw)));
					const double PitchChange = FMath::Abs(FMath::FindDeltaAngleDegrees(
						ProbePitch, static_cast<double>(Controller->GetControlRotation().Pitch)));
					if (YawChange < 0.1 || PitchChange < 0.1)
					{
						Fail(TEXT("Mapped mouse axes did not change third-person camera yaw and pitch."));
						break;
					}
					Test->AddInfo(FString::Printf(TEXT("PIE session %d: mapped mouse changed yaw %.2f/pitch %.2f degrees."),
						SessionIndex + 1, YawChange, PitchChange));
					ReleaseInput();
					JumpOriginZ = Controller->GetControlledKnight()->GetActorLocation().Z;
					EnterPhase(EKnightTrainingPhase::JumpProbe, CommandTimeoutSeconds);
					SetKey(EKeys::SpaceBar, true);
				}
				break;
			case EKnightTrainingPhase::JumpProbe:
				if (Controller->GetControlledKnight()->GetCharacterMovement()->IsFalling()
					&& Controller->GetControlledKnight()->GetActorLocation().Z >= JumpOriginZ + 30.0)
				{
					SetKey(EKeys::SpaceBar, false);
					Test->AddInfo(TEXT("Mapped Space input launched the canonical Knight through CharacterMovement."));
					EnterPhase(EKnightTrainingPhase::WaitLanding, CommandTimeoutSeconds);
				}
				break;
			case EKnightTrainingPhase::WaitLanding:
				if (Controller->GetControlledKnight()->GetCharacterMovement()->IsMovingOnGround())
				{
					MarkerIndex = 0;
					EnterPhase(EKnightTrainingPhase::WalkMarker, WalkingTimeoutSeconds);
				}
				break;
			case EKnightTrainingPhase::WalkMarker:
				WalkToMarker();
				break;
			case EKnightTrainingPhase::WaitCohesion:
				WaitForCohesion();
				break;
			case EKnightTrainingPhase::WaitSwitch:
				WaitForSwitch();
				break;
			case EKnightTrainingPhase::WaitRetry:
				WaitForRetry();
				break;
			case EKnightTrainingPhase::StopPIE:
				ReleaseInput();
				Unsubscribe();
				StartCommand.Reset();
				if (GEditor != nullptr)
				{
					GEditor->RequestEndPlayMap();
					EnterPhase(EKnightTrainingPhase::WaitEndPIE, CleanupTimeoutSeconds);
				}
				else
				{
					Phase = EKnightTrainingPhase::Complete;
				}
				break;
			case EKnightTrainingPhase::WaitEndPIE:
				WaitForEndPIE();
				break;
			default:
				break;
			}
			return Phase == EKnightTrainingPhase::Complete;
		}

	private:
		void EnterPhase(EKnightTrainingPhase NextPhase, double Timeout)
		{
			Phase = NextPhase;
			PhaseStarted = FPlatformTime::Seconds();
			PhaseWorldTime = World.IsValid() ? World->GetTimeSeconds() : 0.0;
			PhaseTimeout = Timeout;
		}

		double WorldElapsed() const
		{
			return World.IsValid() ? World->GetTimeSeconds() - PhaseWorldTime : 0.0;
		}

		void Fail(const FString& Message)
		{
			Test->AddError(Message);
			bFailed = true;
			EnterPhase(EKnightTrainingPhase::StopPIE, CleanupTimeoutSeconds);
		}

		void StartSession()
		{
			if (GEditor == nullptr)
			{
				Fail(TEXT("Editor disappeared before the PIE session could start."));
				return;
			}
			if (!StartCommand.IsValid())
			{
				PlaySettings.Reset(DuplicateObject<ULevelEditorPlaySettings>(
					GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
				PlaySettings->SetPlayNetMode(PIE_Standalone);
				PlaySettings->SetPlayNumberOfClients(1);
				PlaySettings->SetRunUnderOneProcess(true);
				PlaySettings->bLaunchSeparateServer = false;
				PlaySettings->AddToRoot();
				FRequestPlaySessionParams Request;
				Request.EditorPlaySettings = PlaySettings.Get();
				Request.bAllowOnlineSubsystem = false;
				StartCommand = MakeUnique<FStartPIEForAutomationCommand>(Request);
				Test->AddInfo(FString::Printf(TEXT("Starting real solo PIE session %d on %s."),
					SessionIndex + 1, *MapPath));
			}
			if (StartCommand->Update())
			{
				StartCommand.Reset();
				EnterPhase(EKnightTrainingPhase::WaitReady, StartupTimeoutSeconds);
			}
		}

		void WaitForReady()
		{
			UWorld* PIEWorld = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
			if (PIEWorld == nullptr || !PIEWorld->HasBegunPlay())
			{
				return;
			}
			World = PIEWorld;
			GameState = PIEWorld->GetGameState<AKnightGameState>();
			Controller = Cast<AKnightPlayerController>(PIEWorld->GetFirstPlayerController());
			Party = GameState.IsValid() ? GameState->GetPartyState() : nullptr;
			HUD = Controller.IsValid() ? Cast<AKnightTrainingHUD>(Controller->GetHUD()) : nullptr;
			if (!GameState.IsValid() || !Controller.IsValid() || !Party.IsValid()
				|| !HUD.IsValid() || !Party->IsCanonicalRosterReady() || Controller->GetControlledKnight() == nullptr)
			{
				return;
			}
			UKnightInputConfig* Config = Controller->GetKnightInputConfig();
			ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
			UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer != nullptr
				? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
			if (Config == nullptr || Input == nullptr || !Input->HasMappingContext(Config->GetMappingContext()))
			{
				return;
			}
			if (WorldElapsed() < InputSettleSeconds)
			{
				return;
			}
			if (!CaptureCanonicalState())
			{
				return;
			}
			if (!ValidateHUDSource() || HUD->IsDisplayingCompletion())
			{
				Fail(TEXT("The native HUD initial snapshot does not reflect the controlled Knight and fresh training state."));
				return;
			}
			Markers.SetNum(AKnightGameState::RequiredMarkerCount);
			int32 MarkerCount = 0;
			for (TActorIterator<AKnightTrainingMarker> Iterator(PIEWorld); Iterator; ++Iterator)
			{
				const int32 Index = Iterator->MarkerIndex;
				if (!Markers.IsValidIndex(Index) || Markers[Index].IsValid())
				{
					Fail(TEXT("Training map has an invalid or duplicate authored marker index."));
					return;
				}
				Markers[Index] = *Iterator;
				++MarkerCount;
			}
			if (MarkerCount != AKnightGameState::RequiredMarkerCount
				|| GameState->GetTrainingProgress().MarkerMask != 0 || GameState->GetTrainingProgress().bCompleted)
			{
				Fail(TEXT("PIE must begin with exactly three physical markers and fresh training progression."));
				return;
			}
			for (const TWeakObjectPtr<AKnightCharacter>& PreviousKnight : PreviousSessionKnights)
			{
				if (PreviousKnight.IsValid())
				{
					Fail(TEXT("An actor from the previous PIE session survived into the restarted session."));
					return;
				}
			}
			const TSharedRef<FKnightTrainingPlaythroughCommand> Self =
				StaticCastSharedRef<FKnightTrainingPlaythroughCommand>(AsShared());
			CommandHandle = Controller->OnCommandResult.AddSP(Self,
				&FKnightTrainingPlaythroughCommand::HandleCommandResult);
			SourceHandle = Controller->OnControlledKnightChanged.AddSP(Self,
				&FKnightTrainingPlaythroughCommand::HandleControlledKnightChanged);
			CommandCount = 0;
			SourceChanges = 0;
			ProbeOrigin = Controller->GetControlledKnight()->GetActorLocation();
			Test->AddInfo(FString::Printf(TEXT("PIE session %d: canonical roster 3, human 1, running StateTree companions 2."),
				SessionIndex + 1));
			EnterPhase(EKnightTrainingPhase::MoveProbe, CommandTimeoutSeconds);
			SetKey(EKeys::W, true);
		}

		bool CaptureCanonicalState()
		{
			Knights.Reset();
			KnightIds.Reset();
			HeroIds.Reset();
			LastPositions.Reset();
			LastOwners.Reset();
			CompanionTravel.Init(0.0, UKnightSettings::CanonicalPartySize);
			for (int32 Slot = 0; Slot < UKnightSettings::CanonicalPartySize; ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || !IsValid(Entry->Knight) || !Entry->KnightId.IsValid())
				{
					Fail(TEXT("Canonical roster contains a missing actor or invalid stable instance identity."));
					return false;
				}
				Knights.Add(Entry->Knight.Get());
				KnightIds.Add(Entry->KnightId);
				HeroIds.Add(Entry->HeroDefinitionId);
				LastPositions.Add(Entry->Knight->GetActorLocation());
				LastOwners.Add(Entry->OwnerKind);
				Test->AddInfo(FString::Printf(TEXT("Slot %d: actor=%s entity=%s hero=%s."), Slot,
					*Entry->Knight->GetPathName(), *Entry->KnightId.Value.ToString(), *Entry->HeroDefinitionId.ToString()));
			}
			return ValidateCanonicalState();
		}

		bool ValidateCanonicalState()
		{
			int32 ActorCount = 0;
			for (TActorIterator<AKnightCharacter> Iterator(World.Get()); Iterator; ++Iterator)
			{
				++ActorCount;
			}
			if (ActorCount != UKnightSettings::CanonicalPartySize
				|| Party->GetRoster().Num() != UKnightSettings::CanonicalPartySize || Party->GetHumanCount() != 1)
			{
				Fail(TEXT("The solo world lost the invariant of exactly three canonical actors and one human."));
				return false;
			}
			TSet<FGuid> UniqueIds;
			int32 CompanionCount = 0;
			for (int32 Slot = 0; Slot < UKnightSettings::CanonicalPartySize; ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || !Knights[Slot].IsValid() || Entry->Knight != Knights[Slot].Get()
					|| !(Entry->KnightId == KnightIds[Slot]) || !(Entry->Knight->GetKnightEntityId() == KnightIds[Slot])
					|| Entry->HeroDefinitionId != HeroIds[Slot] || UniqueIds.Contains(Entry->KnightId.Value))
				{
					Fail(TEXT("A control transfer/retry replaced an actor, changed identity/definition, or duplicated a GUID."));
					return false;
				}
				UniqueIds.Add(Entry->KnightId.Value);
				if (Entry->OwnerKind == EKnightControllerOwner::Companion)
				{
					AKnightCompanionController* Companion = Cast<AKnightCompanionController>(Entry->Knight->GetController());
					if (Companion == nullptr || !Companion->IsFollowing())
					{
						Fail(FString::Printf(TEXT("Slot %d is missing the running native StateTree companion controller."), Slot));
						return false;
					}
					++CompanionCount;
				}
			}
			if (CompanionCount != UKnightSettings::CanonicalPartySize - 1)
			{
				Fail(TEXT("A human control transfer did not leave exactly two StateTree-controlled companions."));
				return false;
			}
			return true;
		}

		bool ValidateHUDSource() const
		{
			return HUD.IsValid() && Controller.IsValid() && Controller->GetControlledKnight() != nullptr
				&& HUD->GetDisplayedKnightName().EqualTo(Controller->GetControlledKnight()->GetKnightDisplayName());
		}

		void TrackPhysicalMotion()
		{
			if (!Party.IsValid() || Knights.Num() != UKnightSettings::CanonicalPartySize)
			{
				return;
			}
			for (int32 Slot = 0; Slot < Knights.Num(); ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || !Knights[Slot].IsValid())
				{
					continue;
				}
				const FVector Position = Knights[Slot]->GetActorLocation();
				const double Travel = FVector::Dist2D(Position, LastPositions[Slot]);
				if (Entry->OwnerKind == EKnightControllerOwner::Companion
					&& LastOwners[Slot] == EKnightControllerOwner::Companion)
				{
					CompanionTravel[Slot] += Travel;
				}
				else if (Entry->OwnerKind == EKnightControllerOwner::Human && LastOwners[Slot] == Entry->OwnerKind)
				{
					HumanTravel += Travel;
				}
				LastPositions[Slot] = Position;
				LastOwners[Slot] = Entry->OwnerKind;
			}
		}

		void WalkToMarker()
		{
			if (!Controller.IsValid() || !GameState.IsValid() || !Markers[MarkerIndex].IsValid())
			{
				Fail(TEXT("A live controller, training state, or physical marker disappeared during the walk."));
				return;
			}
			if ((GameState->GetTrainingProgress().MarkerMask & (1 << MarkerIndex)) != 0)
			{
				ReleaseInput();
				Test->AddInfo(FString::Printf(TEXT("Physical overlap reached marker %d; actual marker mask=%u."),
					MarkerIndex + 1, static_cast<uint32>(GameState->GetTrainingProgress().MarkerMask)));
				EnterPhase(EKnightTrainingPhase::WaitCohesion, CohesionTimeoutSeconds);
				return;
			}
			AKnightCharacter* Knight = Controller->GetControlledKnight();
			if (Knight == nullptr || Knight->GetCharacterMovement()->MovementMode == MOVE_None)
			{
				Fail(TEXT("The selected Knight cannot physically walk the training route."));
				return;
			}
			FVector Direction = Markers[MarkerIndex]->GetActorLocation() - Knight->GetActorLocation();
			Direction.Z = 0.0;
			Direction.Normalize();
			const FRotationMatrix Basis(FRotator(0.0, Controller->GetControlRotation().Yaw, 0.0));
			const double Forward = FVector::DotProduct(Direction, Basis.GetUnitAxis(EAxis::X));
			const double Right = FVector::DotProduct(Direction, Basis.GetUnitAxis(EAxis::Y));
			SetKey(EKeys::W, Forward > 0.2);
			SetKey(EKeys::S, Forward < -0.2);
			SetKey(EKeys::D, Right > 0.2);
			SetKey(EKeys::A, Right < -0.2);
		}

		void WaitForCohesion()
		{
			if (!ValidateCanonicalState())
			{
				return;
			}
			bool bAllInFormation = true;
			for (const FKnightRosterEntry& Entry : Party->GetRoster())
			{
				if (Entry.OwnerKind != EKnightControllerOwner::Companion)
				{
					continue;
				}
				AKnightCompanionController* Companion = Cast<AKnightCompanionController>(Entry.Knight->GetController());
				FVector Goal;
				float AcceptanceRadius = 0.0f;
				if (!Companion->TryGetFormationGoal(Goal, AcceptanceRadius))
				{
					Fail(TEXT("Running companion StateTree cannot resolve a valid formation goal."));
					return;
				}
				const double Tolerance = AcceptanceRadius
					+ Entry.Knight->GetCapsuleComponent()->GetScaledCapsuleRadius() + FormationMarginCm;
				bAllInFormation &= FVector::Dist2D(Goal, Entry.Knight->GetActorLocation()) <= Tolerance;
			}
			if (!bAllInFormation || WorldElapsed() < InputSettleSeconds)
			{
				return;
			}
			if (MarkerIndex == 0 && (CompanionTravel[1] < MinimumMotionCm || CompanionTravel[2] < MinimumMotionCm))
			{
				Fail(TEXT("The two original AI companions did not physically follow the first training leg."));
				return;
			}
			Test->AddInfo(FString::Printf(TEXT("Marker %d: both StateTree companions converged to real formation goals."),
				MarkerIndex + 1));
			if (MarkerIndex + 1 < AKnightGameState::RequiredMarkerCount)
			{
				BeginSwitch(MarkerIndex + 1, MarkerIndex == 0 ? EKeys::Two : EKeys::Three,
					EKnightTrainingSwitchPurpose::NextMarker);
				return;
			}
			const FKnightTrainingProgress& Progress = GameState->GetTrainingProgress();
			const uint8 RequiredMask = static_cast<uint8>((1 << UKnightSettings::CanonicalPartySize) - 1);
			if (!Progress.bCompleted || Progress.MarkerMask != RequiredMask || Progress.ControlledMask != RequiredMask)
			{
				Fail(TEXT("Walking all three overlaps and selecting all Knights did not complete actual training state."));
				return;
			}
			if (!ValidateHUDSource() || !HUD->IsDisplayingCompletion())
			{
				Fail(TEXT("Actual training completion did not reach the event-driven HUD presentation."));
				return;
			}
			Test->AddInfo(FString::Printf(
				TEXT("PIE session %d COMPLETE: human path %.2f cm; AI travel slots0/1/2 %.2f/%.2f/%.2f cm; revision=%d."),
				SessionIndex + 1, HumanTravel, CompanionTravel[0], CompanionTravel[1], CompanionTravel[2], Progress.Revision));
			BeginSwitch(0, EKeys::One, EKnightTrainingSwitchPurpose::SelectFirst);
		}

		void BeginSwitch(int32 Slot, const FKey& Key, EKnightTrainingSwitchPurpose Purpose)
		{
			ReleaseInput();
			DesiredSlot = Slot;
			SwitchPurpose = Purpose;
			CommandBaseline = CommandCount;
			SourceBaseline = SourceChanges;
			SwitchPositions.Reset();
			for (const TWeakObjectPtr<AKnightCharacter>& Knight : Knights)
			{
				SwitchPositions.Add(Knight->GetActorLocation());
			}
			PulseKey = Key;
			EnterPhase(EKnightTrainingPhase::WaitSwitch, CommandTimeoutSeconds);
			SetKey(PulseKey, true);
		}

		void WaitForSwitch()
		{
			if (CommandCount == CommandBaseline)
			{
				return;
			}
			SetKey(PulseKey, false);
			if (CommandCount != CommandBaseline + 1 || !LastCommandResult.IsSuccess()
				|| Party->GetControlledSlot(Controller.Get()) != DesiredSlot)
			{
				Fail(FString::Printf(TEXT("Mapped %s switch did not produce one successful outcome for slot %d: %s."),
					*PulseKey.ToString(), DesiredSlot, *LastCommandResult.Message.ToString()));
				return;
			}
			if (!ValidateCanonicalState())
			{
				return;
			}
			if (!ValidateHUDSource())
			{
				Fail(TEXT("Possession changed but the HUD retained a different Knight's display name."));
				return;
			}
			for (int32 Slot = 0; Slot < Knights.Num(); ++Slot)
			{
				const double MaximumMovement = Knights[Slot]->GetCharacterMovement()->MaxWalkSpeed
					* (WorldElapsed() + 0.05) + FormationMarginCm;
				if (FVector::Dist2D(Knights[Slot]->GetActorLocation(), SwitchPositions[Slot]) > MaximumMovement)
				{
					Fail(TEXT("A control transfer teleported a canonical actor beyond plausible CharacterMovement motion."));
					return;
				}
			}
			if (SourceChanges - SourceBaseline < 1 || SourceChanges - SourceBaseline > 2)
			{
				Fail(TEXT("Control source change notifications were missing or duplicated during a single switch."));
				return;
			}
			Test->AddInfo(FString::Printf(TEXT("Mapped %s selected slot %d; original actors/GUIDs/definitions preserved."),
				*PulseKey.ToString(), DesiredSlot));
			if (SwitchPurpose == EKnightTrainingSwitchPurpose::NextMarker)
			{
				++MarkerIndex;
				for (int32 Slot = 0; Slot < Knights.Num(); ++Slot)
				{
					LastPositions[Slot] = Knights[Slot]->GetActorLocation();
					LastOwners[Slot] = Party->FindSlot(Slot)->OwnerKind;
				}
				EnterPhase(EKnightTrainingPhase::WalkMarker, WalkingTimeoutSeconds);
			}
			else if (SwitchPurpose == EKnightTrainingSwitchPurpose::SelectFirst)
			{
				BeginSwitch(1, EKeys::Tab, EKnightTrainingSwitchPurpose::CycleNext);
			}
			else
			{
				CommandBaseline = CommandCount;
				ProgressRevisionBeforeRetry = GameState->GetTrainingProgress().Revision;
				bRetryObserved = false;
				EnterPhase(EKnightTrainingPhase::WaitRetry, CommandTimeoutSeconds);
				SetKey(EKeys::W, true);
				SetKey(EKeys::R, true);
			}
		}

		void WaitForRetry()
		{
			if (CommandCount == CommandBaseline)
			{
				return;
			}
			if (!bRetryObserved)
			{
				SetKey(EKeys::R, false);
				const FKnightTrainingProgress& Progress = GameState->GetTrainingProgress();
				const int32 Slot = Party->GetControlledSlot(Controller.Get());
				if (CommandCount != CommandBaseline + 1 || !LastCommandResult.IsSuccess()
					|| Progress.MarkerMask != 0 || Progress.bCompleted
					|| Progress.ControlledMask != static_cast<uint8>(1 << Slot)
					|| Progress.Revision <= ProgressRevisionBeforeRetry || !ValidateCanonicalState())
				{
					Fail(TEXT("Mapped R retry failed to reset actual progress while preserving canonical identity."));
					return;
				}
				if (!ValidateHUDSource() || HUD->IsDisplayingCompletion())
				{
					Fail(TEXT("Mapped R reset actual progression but did not reset the cached HUD completion state."));
					return;
				}
				AKnightPartyGameMode* GameMode = World->GetAuthGameMode<AKnightPartyGameMode>();
				if (GameMode == nullptr || FVector::Dist2D(Controller->GetControlledKnight()->GetActorLocation(),
					GameMode->GetSpawnLocationForSlot(Slot)) > FormationMarginCm)
				{
					Fail(TEXT("Retry did not restore the controlled Knight to its authored training spawn."));
					return;
				}
				RetryOrigin = Controller->GetControlledKnight()->GetActorLocation();
				PhaseWorldTime = World->GetTimeSeconds();
				bRetryObserved = true;
			}
			if (WorldElapsed() < InputSettleSeconds)
			{
				return;
			}
			const double Drift = FVector::Dist2D(RetryOrigin, Controller->GetControlledKnight()->GetActorLocation());
			if (Drift > RetryDriftToleranceCm || GameState->GetTrainingProgress().MarkerMask != 0)
			{
				Fail(FString::Printf(TEXT("Held W leaked across retry or progress reset: post-retry drift %.2f cm."), Drift));
				return;
			}
			Test->AddInfo(FString::Printf(TEXT("PIE session %d: mapped R reset progress; held W flushed, drift %.2f cm."),
				SessionIndex + 1, Drift));
			EnterPhase(EKnightTrainingPhase::StopPIE, CleanupTimeoutSeconds);
		}

		void WaitForEndPIE()
		{
			if (GEditor == nullptr || GEngine == nullptr)
			{
				Phase = EKnightTrainingPhase::Complete;
				return;
			}
			bool bHasPIEWorld = false;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				bHasPIEWorld |= Context.WorldType == EWorldType::PIE && Context.World() != nullptr;
			}
			if (bHasPIEWorld || GEditor->PlayWorld != nullptr)
			{
				if (FPlatformTime::Seconds() - PhaseStarted > CleanupTimeoutSeconds)
				{
					Test->AddError(TEXT("PIE cleanup did not finish within ten seconds; forcing synchronous EndPlayMap."));
					bFailed = true;
					GEditor->EndPlayMap();
					Phase = EKnightTrainingPhase::Complete;
				}
				return;
			}
			Test->AddInfo(FString::Printf(TEXT("PIE session %d ended cleanly; no live PIE world remains."), SessionIndex + 1));
			PreviousSessionKnights = Knights;
			World.Reset();
			Controller.Reset();
			Party.Reset();
			GameState.Reset();
			HUD.Reset();
			PlaySettings.Reset();
			if (!bFailed && SessionIndex == 0)
			{
				++SessionIndex;
				HumanTravel = 0.0;
				EnterPhase(EKnightTrainingPhase::StartPIE, StartupTimeoutSeconds);
			}
			else
			{
				Phase = EKnightTrainingPhase::Complete;
			}
		}

		void SetKey(const FKey& Key, bool bIsDown)
		{
			if (!Controller.IsValid() || DownKeys.Contains(Key) == bIsDown)
			{
				return;
			}
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key,
				bIsDown ? IE_Pressed : IE_Released, bIsDown ? 1.0f : 0.0f));
			if (bIsDown)
			{
				DownKeys.Add(Key);
			}
			else
			{
				DownKeys.Remove(Key);
			}
		}

		void InjectAxis(const FKey& Key, float Value)
		{
			if (Controller.IsValid())
			{
				Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Axis, Value, 1));
			}
		}

		void ReleaseInput()
		{
			const TArray<FKey> Keys = DownKeys.Array();
			for (const FKey& Key : Keys)
			{
				SetKey(Key, false);
			}
			DownKeys.Reset();
			InjectAxis(EKeys::MouseX, 0.0f);
			InjectAxis(EKeys::MouseY, 0.0f);
		}

		void Unsubscribe()
		{
			if (Controller.IsValid())
			{
				Controller->OnCommandResult.Remove(CommandHandle);
				Controller->OnControlledKnightChanged.Remove(SourceHandle);
			}
			CommandHandle.Reset();
			SourceHandle.Reset();
		}

		void HandleCommandResult(const FKnightResult& Result)
		{
			++CommandCount;
			LastCommandResult = Result;
		}

		void HandleControlledKnightChanged(AKnightCharacter* Previous, AKnightCharacter* Current)
		{
			if (Previous != Current)
			{
				++SourceChanges;
			}
		}

		FAutomationTestBase* Test;
		EKnightTrainingPhase Phase = EKnightTrainingPhase::WaitNavigation;
		EKnightTrainingSwitchPurpose SwitchPurpose = EKnightTrainingSwitchPurpose::NextMarker;
		double RunStarted = 0.0;
		double PhaseStarted = 0.0;
		double PhaseWorldTime = 0.0;
		double PhaseTimeout = StartupTimeoutSeconds;
		double ProbeYaw = 0.0;
		double ProbePitch = 0.0;
		double JumpOriginZ = 0.0;
		double HumanTravel = 0.0;
		int32 SessionIndex = 0;
		int32 MarkerIndex = 0;
		int32 DesiredSlot = INDEX_NONE;
		int32 CommandCount = 0;
		int32 CommandBaseline = 0;
		int32 SourceChanges = 0;
		int32 SourceBaseline = 0;
		int32 ProgressRevisionBeforeRetry = 0;
		bool bFailed = false;
		bool bRetryObserved = false;
		FVector ProbeOrigin = FVector::ZeroVector;
		FVector RetryOrigin = FVector::ZeroVector;
		FKey PulseKey;
		FKnightResult LastCommandResult;
		FDelegateHandle CommandHandle;
		FDelegateHandle SourceHandle;
		TSet<FKey> DownKeys;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AKnightGameState> GameState;
		TWeakObjectPtr<AKnightPlayerController> Controller;
		TWeakObjectPtr<AKnightPartyState> Party;
		TWeakObjectPtr<AKnightTrainingHUD> HUD;
		TArray<TWeakObjectPtr<AKnightCharacter>> Knights;
		TArray<TWeakObjectPtr<AKnightCharacter>> PreviousSessionKnights;
		TArray<TWeakObjectPtr<AKnightTrainingMarker>> Markers;
		TArray<FKnightEntityId> KnightIds;
		TArray<FPrimaryAssetId> HeroIds;
		TArray<FVector> LastPositions;
		TArray<FVector> SwitchPositions;
		TArray<EKnightControllerOwner> LastOwners;
		TArray<double> CompanionTravel;
		TStrongObjectPtr<ULevelEditorPlaySettings> PlaySettings;
		TUniquePtr<FStartPIEForAutomationCommand> StartCommand;
		FWaitForEditorNavMeshBuildCommand NavigationWait;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightTrainingPlaythroughTest, "Knight.Framework.TrainingPlaythrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightTrainingPlaythroughTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->PlayWorld != nullptr)
	{
		AddError(TEXT("Training playthrough requires an Editor with no existing PIE session."));
		return false;
	}
	if (!FPackageName::DoesPackageExist(KnightTrainingIntegration::MapPath))
	{
		AddError(TEXT("The real L_M01_TrainingYard map is missing; run KnightBootstrap after the integrated build."));
		return false;
	}
	if (!FEditorFileUtils::LoadMap(*KnightTrainingIntegration::MapPath, false, false))
	{
		AddError(TEXT("The real training map could not be opened by the installed Editor."));
		return false;
	}
	AddInfo(TEXT("Training gate uses mapped W/A/S/D, MouseX, 1/2/3/Tab and R; no teleport or direct progress writes."));
	AddCommand(new KnightTrainingIntegration::FKnightTrainingPlaythroughCommand(this));
	return true;
}

#endif
