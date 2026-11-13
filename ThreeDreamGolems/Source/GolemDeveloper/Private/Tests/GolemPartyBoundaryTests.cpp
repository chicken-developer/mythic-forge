#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/GolemCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "FileHelpers.h"
#include "Framework/GolemGameState.h"
#include "Framework/GolemPartyGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "Input/GolemInputConfig.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/PackageName.h"
#include "Party/GolemPartyState.h"
#include "Player/GolemPlayerController.h"
#include "Settings/GolemSettings.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace GolemPartyBoundary
{
	const FString MapPath = TEXT("/Game/Golem/Maps/Playables/L_M01_TrainingYard");
	const FVector FallFaultLocation(2400.0, -900.0, -100.0);
	constexpr double StartupTimeoutSeconds = 30.0;
	constexpr double WallTimeoutSeconds = 20.0;
	constexpr double RecoveryTimeoutSeconds = 10.0;
	constexpr double CleanupTimeoutSeconds = 10.0;
	constexpr double WallHoldSeconds = 3.0;
	constexpr double SettleSeconds = 0.3;
	constexpr double WallInsideX = 1900.0;
	constexpr double WallContactToleranceCm = 10.0;
	constexpr double WallDriftToleranceCm = 5.0;
	constexpr double MinimumApproachDistanceCm = 1000.0;
	constexpr double SpawnToleranceCm = 30.0;
	constexpr double ExpectedKillZ = -2000.0;

	enum class EGolemBoundaryPhase : uint8
	{
		WaitNavigation,
		StartPIE,
		WaitReady,
		WalkWall,
		HoldWall,
		WaitRecovery,
		VerifyRecovery,
		StopPIE,
		WaitEndPIE,
		Complete
	};

	struct FGolemBoundarySnapshot
	{
		TWeakObjectPtr<AGolemCharacter> Golem;
		TWeakObjectPtr<AController> Controller;
		FGolemEntityId GolemId;
		FPrimaryAssetId HeroId;
		EGolemControllerOwner OwnerKind = EGolemControllerOwner::Unassigned;
	};

	/** Boundary movement is real input. The separate fall teleport is explicit fault injection. */
	class FGolemPartyBoundaryCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FGolemPartyBoundaryCommand(FAutomationTestBase* InTest)
			: Test(InTest), PhaseStarted(FPlatformTime::Seconds())
		{
		}

		virtual ~FGolemPartyBoundaryCommand() override
		{
			ReleaseInput();
			StartCommand.Reset();
			if (bOwnsSession && GEditor != nullptr && GEditor->PlayWorld != nullptr)
			{
				GEditor->RequestEndPlayMap();
			}
		}

		virtual bool Update() override
		{
			if (Phase == EGolemBoundaryPhase::Complete)
			{
				return true;
			}
			if (!bFailed && Test->HasAnyErrors())
			{
				Fail(TEXT("Unreal reported an error during the boundary/fall regression; cleaning up PIE."));
			}
			if (Phase < EGolemBoundaryPhase::StopPIE && FPlatformTime::Seconds() - PhaseStarted > PhaseTimeout)
			{
				Fail(FString::Printf(TEXT("Boundary/fall regression timed out in phase %d."), static_cast<int32>(Phase)));
			}
			if (Phase >= EGolemBoundaryPhase::WalkWall && Phase <= EGolemBoundaryPhase::VerifyRecovery
				&& (!World.IsValid() || !State.IsValid() || !Party.IsValid()
					|| !Player.IsValid() || !ControlledGolem.IsValid()))
			{
				Fail(TEXT("The boundary/fall regression lost its live world, party, human or canonical Golem."));
			}

			switch (Phase)
			{
			case EGolemBoundaryPhase::WaitNavigation:
				if (NavigationWait.Update())
				{
					EnterPhase(EGolemBoundaryPhase::StartPIE, StartupTimeoutSeconds);
				}
				break;
			case EGolemBoundaryPhase::StartPIE:
				StartSession();
				break;
			case EGolemBoundaryPhase::WaitReady:
				WaitForReady();
				break;
			case EGolemBoundaryPhase::WalkWall:
				WalkIntoWall();
				break;
			case EGolemBoundaryPhase::HoldWall:
				HoldAgainstWall();
				break;
			case EGolemBoundaryPhase::WaitRecovery:
				WaitForRecovery();
				break;
			case EGolemBoundaryPhase::VerifyRecovery:
				VerifyRecovery();
				break;
			case EGolemBoundaryPhase::StopPIE:
				ReleaseInput();
				StartCommand.Reset();
				if (bOwnsSession && GEditor != nullptr)
				{
					GEditor->RequestEndPlayMap();
					EnterPhase(EGolemBoundaryPhase::WaitEndPIE, CleanupTimeoutSeconds);
				}
				else
				{
					Phase = EGolemBoundaryPhase::Complete;
				}
				break;
			case EGolemBoundaryPhase::WaitEndPIE:
				WaitForEndPIE();
				break;
			default:
				break;
			}
			return Phase == EGolemBoundaryPhase::Complete;
		}

	private:
		void EnterPhase(EGolemBoundaryPhase InPhase, double Timeout)
		{
			Phase = InPhase;
			PhaseTimeout = Timeout;
			PhaseStarted = FPlatformTime::Seconds();
			PhaseWorldTime = World.IsValid() ? World->GetTimeSeconds() : 0.0;
		}

		double WorldElapsed() const
		{
			return World.IsValid() ? World->GetTimeSeconds() - PhaseWorldTime : 0.0;
		}

		void Fail(const FString& Message)
		{
			Test->AddError(Message);
			bFailed = true;
			EnterPhase(EGolemBoundaryPhase::StopPIE, CleanupTimeoutSeconds);
		}

		void StartSession()
		{
			if (GEditor == nullptr)
			{
				Fail(TEXT("Editor is unavailable for the boundary regression."));
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
				bOwnsSession = true;
			}
			if (StartCommand->Update())
			{
				StartCommand.Reset();
				EnterPhase(EGolemBoundaryPhase::WaitReady, StartupTimeoutSeconds);
			}
		}

		void WaitForReady()
		{
			World = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
			if (!World.IsValid() || !World->HasBegunPlay())
			{
				return;
			}
			State = World->GetGameState<AGolemGameState>();
			Party = State.IsValid() ? State->GetPartyState() : nullptr;
			Player = Cast<AGolemPlayerController>(World->GetFirstPlayerController());
			ControlledGolem = Player.IsValid() ? Player->GetControlledGolem() : nullptr;
			if (!Party.IsValid() || !Party->IsCanonicalRosterReady() || !ControlledGolem.IsValid())
			{
				return;
			}
			ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
			UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer != nullptr
				? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
			UGolemInputConfig* Config = Player->GetGolemInputConfig();
			if (Config == nullptr || Input == nullptr || !Input->HasMappingContext(Config->GetMappingContext())
				|| !ControlledGolem->GetCharacterMovement()->IsMovingOnGround() || WorldElapsed() < SettleSeconds)
			{
				return;
			}
			if (!FMath::IsNearlyEqual(static_cast<double>(World->GetWorldSettings()->KillZ), ExpectedKillZ, 1.0))
			{
				Fail(TEXT("The authored training map lacks the bounded -2000 cm KillZ fixture; rerun bootstrap."));
				return;
			}
			for (int32 Slot = 0; Slot < UGolemSettings::CanonicalPartySize; ++Slot)
			{
				const FGolemRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || !IsValid(Entry->Golem) || !IsValid(Entry->Controller))
				{
					Fail(TEXT("Boundary regression cannot snapshot an incomplete canonical roster."));
					return;
				}
				FGolemBoundarySnapshot& Snapshot = Snapshots.AddDefaulted_GetRef();
				Snapshot.Golem = Entry->Golem.Get();
				Snapshot.Controller = Entry->Controller.Get();
				Snapshot.GolemId = Entry->GolemId;
				Snapshot.HeroId = Entry->HeroDefinitionId;
				Snapshot.OwnerKind = Entry->OwnerKind;
			}
			RosterRevision = Party->GetRosterRevision();
			ControlledSlot = Party->GetControlledSlot(Player.Get());
			if (!ValidateCanonicalState())
			{
				return;
			}
			ExpectedWallStopX = WallInsideX - ControlledGolem->GetCapsuleComponent()->GetScaledCapsuleRadius();
			WallStartX = ControlledGolem->GetActorLocation().X;
			Player->SetControlRotation(FRotator::ZeroRotator);
			Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W, IE_Pressed, 1.0f));
			bMoveKeyDown = true;
			Test->AddInfo(TEXT("Walking the original controlled Golem from spawn into the real east wall with mapped W."));
			EnterPhase(EGolemBoundaryPhase::WalkWall, WallTimeoutSeconds);
		}

		bool ValidateCanonicalState()
		{
			int32 ActorCount = 0;
			for (TActorIterator<AGolemCharacter> Iterator(World.Get()); Iterator; ++Iterator)
			{
				++ActorCount;
			}
			if (Snapshots.Num() != UGolemSettings::CanonicalPartySize || ActorCount != UGolemSettings::CanonicalPartySize
				|| Party->GetRoster().Num() != UGolemSettings::CanonicalPartySize || Party->GetHumanCount() != 1
				|| Party->GetRosterRevision() != RosterRevision || Party->GetControlledSlot(Player.Get()) != ControlledSlot
				|| Player->GetPawn() != ControlledGolem.Get())
			{
				Fail(TEXT("Boundary movement/fall changed canonical actor count, roster revision, human slot or possession."));
				return false;
			}
			for (int32 Slot = 0; Slot < Snapshots.Num(); ++Slot)
			{
				const FGolemRosterEntry* Entry = Party->FindSlot(Slot);
				const FGolemBoundarySnapshot& Snapshot = Snapshots[Slot];
				if (Entry == nullptr || !Snapshot.Golem.IsValid() || !Snapshot.Controller.IsValid()
					|| Entry->Golem != Snapshot.Golem.Get() || Entry->GolemId != Snapshot.GolemId
					|| Entry->HeroDefinitionId != Snapshot.HeroId || Entry->OwnerKind != Snapshot.OwnerKind
					|| Entry->Controller != Snapshot.Controller.Get()
					|| Snapshot.Golem->GetGolemEntityId() != Snapshot.GolemId
					|| Snapshot.Golem->GetHeroDefinitionId() != Snapshot.HeroId
					|| Snapshot.Golem->GetController() != Snapshot.Controller.Get()
					|| Snapshot.Controller->GetPawn() != Snapshot.Golem.Get())
				{
					Fail(TEXT("Boundary movement/fall replaced an original Golem, identity, definition or controller."));
					return false;
				}
			}
			return true;
		}

		void WalkIntoWall()
		{
			const FVector Position = ControlledGolem->GetActorLocation();
			if (Position.X > ExpectedWallStopX + WallDriftToleranceCm)
			{
				Fail(TEXT("Mapped forward movement penetrated the authored east wall."));
				return;
			}
			if (Position.X >= ExpectedWallStopX - WallContactToleranceCm
				&& FMath::Abs(ControlledGolem->GetVelocity().X) < WallDriftToleranceCm)
			{
				WallContactX = Position.X;
				if (Position.X - WallStartX < MinimumApproachDistanceCm)
				{
					Fail(TEXT("Boundary probe did not reach the perimeter through actual CharacterMovement."));
					return;
				}
				if (!ValidateCanonicalState())
				{
					return;
				}
				EnterPhase(EGolemBoundaryPhase::HoldWall, WallTimeoutSeconds);
			}
		}

		void HoldAgainstWall()
		{
			const FVector Position = ControlledGolem->GetActorLocation();
			if (Position.X > ExpectedWallStopX + WallDriftToleranceCm
				|| FMath::Abs(Position.X - WallContactX) > WallDriftToleranceCm
				|| !ControlledGolem->GetCharacterMovement()->IsMovingOnGround())
			{
				Fail(TEXT("Three seconds of held W leaked through the wall or dropped the canonical Golem off the yard."));
				return;
			}
			if (WorldElapsed() < WallHoldSeconds)
			{
				return;
			}
			ReleaseInput();
			if (!ValidateCanonicalState())
			{
				return;
			}
			Test->AddInfo(FString::Printf(TEXT("Held mapped W against collision for %.2f world seconds: X=%.2f, limit=%.2f."),
				WorldElapsed(), Position.X, ExpectedWallStopX));
			ProgressBeforeFall = State->GetTrainingProgress();
			// Fault setup bypasses the perimeter. Engine gravity and KillZ must invoke recovery during subsequent world ticks.
			ControlledGolem->GetCharacterMovement()->StopMovementImmediately();
			if (!ControlledGolem->SetActorLocation(FallFaultLocation, false, nullptr, ETeleportType::TeleportPhysics))
			{
				Fail(TEXT("The explicit out-of-yard fault injection could not position the original canonical Golem."));
				return;
			}
			ControlledGolem->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			MinimumFallZ = FallFaultLocation.Z;
			Test->AddInfo(FString::Printf(
				TEXT("Fault injection: original actor=%s at %s; engine gravity must reach KillZ %.0f."),
				*ControlledGolem->GetPathName(), *FallFaultLocation.ToString(), World->GetWorldSettings()->KillZ));
			EnterPhase(EGolemBoundaryPhase::WaitRecovery, RecoveryTimeoutSeconds);
		}

		void WaitForRecovery()
		{
			const FVector Position = ControlledGolem->GetActorLocation();
			MinimumFallZ = FMath::Min(MinimumFallZ, Position.Z);
			bObservedGravity |= Position.Z < FallFaultLocation.Z - 100.0
				&& ControlledGolem->GetCharacterMovement()->IsFalling() && ControlledGolem->GetVelocity().Z < -50.0;
			AGolemPartyGameMode* Mode = World->GetAuthGameMode<AGolemPartyGameMode>();
			if (Mode == nullptr)
			{
				Fail(TEXT("The live party GameMode disappeared during the real KillZ regression."));
				return;
			}
			RecoverySpawn = Mode->GetSpawnLocationForSlot(ControlledSlot);
			if (FVector::Dist(Position, RecoverySpawn) > SpawnToleranceCm
				|| !ControlledGolem->GetCharacterMovement()->IsMovingOnGround())
			{
				return;
			}
			if (!bObservedGravity || MinimumFallZ > ExpectedKillZ + 500.0 || !ValidateCanonicalState())
			{
				Fail(TEXT("Spawn recovery was observed without a real gravity descent near KillZ or intact canonical state."));
				return;
			}
			const FGolemTrainingProgress& Progress = State->GetTrainingProgress();
			if (Progress.Revision != ProgressBeforeFall.Revision || Progress.MarkerMask != ProgressBeforeFall.MarkerMask
				|| Progress.ControlledMask != ProgressBeforeFall.ControlledMask
				|| Progress.bCompleted != ProgressBeforeFall.bCompleted)
			{
				Fail(TEXT("Automatic fall recovery reset training progression instead of preserving existing Golem state."));
				return;
			}
			Test->AddInfo(FString::Printf(
				TEXT("Real gravity/KillZ recovery: lowest sampled Z=%.2f, restored=%s, roster revision=%d."),
				MinimumFallZ, *Position.ToString(), Party->GetRosterRevision()));
			EnterPhase(EGolemBoundaryPhase::VerifyRecovery, RecoveryTimeoutSeconds);
		}

		void VerifyRecovery()
		{
			if (WorldElapsed() < SettleSeconds)
			{
				return;
			}
			if (!ControlledGolem->GetCharacterMovement()->IsMovingOnGround()
				|| FVector::Dist(ControlledGolem->GetActorLocation(), RecoverySpawn) > SpawnToleranceCm)
			{
				Fail(TEXT("The recovered canonical Golem did not remain safely walking at its authored spawn."));
				return;
			}
			if (!ValidateCanonicalState())
			{
				return;
			}
			Test->AddInfo(TEXT("Recovered Golem remained possessed and walking; canonical party state is unchanged."));
			EnterPhase(EGolemBoundaryPhase::StopPIE, CleanupTimeoutSeconds);
		}

		void ReleaseInput()
		{
			if (bMoveKeyDown && Player.IsValid())
			{
				Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W, IE_Released, 0.0f));
				Player->ResetControlInput();
			}
			bMoveKeyDown = false;
		}

		void WaitForEndPIE()
		{
			bool bHasWorld = GEditor != nullptr && GEditor->PlayWorld != nullptr;
			if (GEngine != nullptr)
			{
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					bHasWorld |= Context.WorldType == EWorldType::PIE && Context.World() != nullptr;
				}
			}
			if (bHasWorld && FPlatformTime::Seconds() - PhaseStarted <= CleanupTimeoutSeconds)
			{
				return;
			}
			if (bHasWorld)
			{
				Test->AddError(TEXT("Boundary test PIE teardown timed out; forcing synchronous EndPlayMap."));
				if (GEditor != nullptr)
				{
					GEditor->EndPlayMap();
				}
			}
			else
			{
				Test->AddInfo(TEXT("Boundary/fall PIE session ended cleanly without a live PIE world."));
			}
			bOwnsSession = false;
			PlaySettings.Reset();
			Phase = EGolemBoundaryPhase::Complete;
		}

		FAutomationTestBase* Test;
		EGolemBoundaryPhase Phase = EGolemBoundaryPhase::WaitNavigation;
		double PhaseStarted;
		double PhaseTimeout = StartupTimeoutSeconds;
		double PhaseWorldTime = 0.0;
		double ExpectedWallStopX = 0.0;
		double WallStartX = 0.0;
		double WallContactX = 0.0;
		double MinimumFallZ = 0.0;
		int32 RosterRevision = 0;
		int32 ControlledSlot = INDEX_NONE;
		bool bFailed = false;
		bool bOwnsSession = false;
		bool bMoveKeyDown = false;
		bool bObservedGravity = false;
		FGolemTrainingProgress ProgressBeforeFall;
		FVector RecoverySpawn = FVector::ZeroVector;
		TArray<FGolemBoundarySnapshot> Snapshots;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AGolemGameState> State;
		TWeakObjectPtr<AGolemPartyState> Party;
		TWeakObjectPtr<AGolemPlayerController> Player;
		TWeakObjectPtr<AGolemCharacter> ControlledGolem;
		TStrongObjectPtr<ULevelEditorPlaySettings> PlaySettings;
		TUniquePtr<FStartPIEForAutomationCommand> StartCommand;
		FWaitForEditorNavMeshBuildCommand NavigationWait;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemPartyBoundaryTest, "Golem.Framework.PartyBoundaryRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemPartyBoundaryTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->PlayWorld != nullptr)
	{
		AddError(TEXT("Boundary regression requires an Editor without an existing PIE session."));
		return false;
	}
	if (!FPackageName::DoesPackageExist(GolemPartyBoundary::MapPath)
		|| !FEditorFileUtils::LoadMap(*GolemPartyBoundary::MapPath, false, false))
	{
		AddError(TEXT("The real training map is missing or cannot be loaded; rebuild and bootstrap M1 first."));
		return false;
	}
	AddCommand(new GolemPartyBoundary::FGolemPartyBoundaryCommand(this));
	return true;
}

#endif
