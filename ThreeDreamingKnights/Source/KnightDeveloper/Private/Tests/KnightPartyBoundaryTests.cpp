#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Character/KnightCharacter.h"
#include "Components/CapsuleComponent.h"
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
#include "GameFramework/Controller.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformTime.h"
#include "Input/KnightInputConfig.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/PackageName.h"
#include "Party/KnightPartyState.h"
#include "Player/KnightPlayerController.h"
#include "Settings/KnightSettings.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace KnightPartyBoundary
{
	const FString MapPath = TEXT("/Game/Knight/Maps/Playables/L_M01_TrainingYard");
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

	enum class EKnightBoundaryPhase : uint8
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

	struct FKnightBoundarySnapshot
	{
		TWeakObjectPtr<AKnightCharacter> Knight;
		TWeakObjectPtr<AController> Controller;
		FKnightEntityId KnightId;
		FPrimaryAssetId HeroId;
		EKnightControllerOwner OwnerKind = EKnightControllerOwner::Unassigned;
	};

	/** Boundary movement is real input. The separate fall teleport is explicit fault injection. */
	class FKnightPartyBoundaryCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FKnightPartyBoundaryCommand(FAutomationTestBase* InTest)
			: Test(InTest), PhaseStarted(FPlatformTime::Seconds())
		{
		}

		virtual ~FKnightPartyBoundaryCommand() override
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
			if (Phase == EKnightBoundaryPhase::Complete)
			{
				return true;
			}
			if (!bFailed && Test->HasAnyErrors())
			{
				Fail(TEXT("Unreal reported an error during the boundary/fall regression; cleaning up PIE."));
			}
			if (Phase < EKnightBoundaryPhase::StopPIE && FPlatformTime::Seconds() - PhaseStarted > PhaseTimeout)
			{
				Fail(FString::Printf(TEXT("Boundary/fall regression timed out in phase %d."), static_cast<int32>(Phase)));
			}
			if (Phase >= EKnightBoundaryPhase::WalkWall && Phase <= EKnightBoundaryPhase::VerifyRecovery
				&& (!World.IsValid() || !State.IsValid() || !Party.IsValid()
					|| !Player.IsValid() || !ControlledKnight.IsValid()))
			{
				Fail(TEXT("The boundary/fall regression lost its live world, party, human or canonical Knight."));
			}

			switch (Phase)
			{
			case EKnightBoundaryPhase::WaitNavigation:
				if (NavigationWait.Update())
				{
					EnterPhase(EKnightBoundaryPhase::StartPIE, StartupTimeoutSeconds);
				}
				break;
			case EKnightBoundaryPhase::StartPIE:
				StartSession();
				break;
			case EKnightBoundaryPhase::WaitReady:
				WaitForReady();
				break;
			case EKnightBoundaryPhase::WalkWall:
				WalkIntoWall();
				break;
			case EKnightBoundaryPhase::HoldWall:
				HoldAgainstWall();
				break;
			case EKnightBoundaryPhase::WaitRecovery:
				WaitForRecovery();
				break;
			case EKnightBoundaryPhase::VerifyRecovery:
				VerifyRecovery();
				break;
			case EKnightBoundaryPhase::StopPIE:
				ReleaseInput();
				StartCommand.Reset();
				if (bOwnsSession && GEditor != nullptr)
				{
					GEditor->RequestEndPlayMap();
					EnterPhase(EKnightBoundaryPhase::WaitEndPIE, CleanupTimeoutSeconds);
				}
				else
				{
					Phase = EKnightBoundaryPhase::Complete;
				}
				break;
			case EKnightBoundaryPhase::WaitEndPIE:
				WaitForEndPIE();
				break;
			default:
				break;
			}
			return Phase == EKnightBoundaryPhase::Complete;
		}

	private:
		void EnterPhase(EKnightBoundaryPhase InPhase, double Timeout)
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
			EnterPhase(EKnightBoundaryPhase::StopPIE, CleanupTimeoutSeconds);
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
				EnterPhase(EKnightBoundaryPhase::WaitReady, StartupTimeoutSeconds);
			}
		}

		void WaitForReady()
		{
			World = GEditor != nullptr ? GEditor->PlayWorld.Get() : nullptr;
			if (!World.IsValid() || !World->HasBegunPlay())
			{
				return;
			}
			State = World->GetGameState<AKnightGameState>();
			Party = State.IsValid() ? State->GetPartyState() : nullptr;
			Player = Cast<AKnightPlayerController>(World->GetFirstPlayerController());
			ControlledKnight = Player.IsValid() ? Player->GetControlledKnight() : nullptr;
			if (!Party.IsValid() || !Party->IsCanonicalRosterReady() || !ControlledKnight.IsValid())
			{
				return;
			}
			ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
			UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer != nullptr
				? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
			UKnightInputConfig* Config = Player->GetKnightInputConfig();
			if (Config == nullptr || Input == nullptr || !Input->HasMappingContext(Config->GetMappingContext())
				|| !ControlledKnight->GetCharacterMovement()->IsMovingOnGround() || WorldElapsed() < SettleSeconds)
			{
				return;
			}
			if (!FMath::IsNearlyEqual(static_cast<double>(World->GetWorldSettings()->KillZ), ExpectedKillZ, 1.0))
			{
				Fail(TEXT("The authored training map lacks the bounded -2000 cm KillZ fixture; rerun bootstrap."));
				return;
			}
			for (int32 Slot = 0; Slot < UKnightSettings::CanonicalPartySize; ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				if (Entry == nullptr || !IsValid(Entry->Knight) || !IsValid(Entry->Controller))
				{
					Fail(TEXT("Boundary regression cannot snapshot an incomplete canonical roster."));
					return;
				}
				FKnightBoundarySnapshot& Snapshot = Snapshots.AddDefaulted_GetRef();
				Snapshot.Knight = Entry->Knight.Get();
				Snapshot.Controller = Entry->Controller.Get();
				Snapshot.KnightId = Entry->KnightId;
				Snapshot.HeroId = Entry->HeroDefinitionId;
				Snapshot.OwnerKind = Entry->OwnerKind;
			}
			RosterRevision = Party->GetRosterRevision();
			ControlledSlot = Party->GetControlledSlot(Player.Get());
			if (!ValidateCanonicalState())
			{
				return;
			}
			ExpectedWallStopX = WallInsideX - ControlledKnight->GetCapsuleComponent()->GetScaledCapsuleRadius();
			WallStartX = ControlledKnight->GetActorLocation().X;
			Player->SetControlRotation(FRotator::ZeroRotator);
			Player->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W, IE_Pressed, 1.0f));
			bMoveKeyDown = true;
			Test->AddInfo(TEXT("Walking the original controlled Knight from spawn into the real east wall with mapped W."));
			EnterPhase(EKnightBoundaryPhase::WalkWall, WallTimeoutSeconds);
		}

		bool ValidateCanonicalState()
		{
			int32 ActorCount = 0;
			for (TActorIterator<AKnightCharacter> Iterator(World.Get()); Iterator; ++Iterator)
			{
				++ActorCount;
			}
			if (Snapshots.Num() != UKnightSettings::CanonicalPartySize || ActorCount != UKnightSettings::CanonicalPartySize
				|| Party->GetRoster().Num() != UKnightSettings::CanonicalPartySize || Party->GetHumanCount() != 1
				|| Party->GetRosterRevision() != RosterRevision || Party->GetControlledSlot(Player.Get()) != ControlledSlot
				|| Player->GetPawn() != ControlledKnight.Get())
			{
				Fail(TEXT("Boundary movement/fall changed canonical actor count, roster revision, human slot or possession."));
				return false;
			}
			for (int32 Slot = 0; Slot < Snapshots.Num(); ++Slot)
			{
				const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
				const FKnightBoundarySnapshot& Snapshot = Snapshots[Slot];
				if (Entry == nullptr || !Snapshot.Knight.IsValid() || !Snapshot.Controller.IsValid()
					|| Entry->Knight != Snapshot.Knight.Get() || Entry->KnightId != Snapshot.KnightId
					|| Entry->HeroDefinitionId != Snapshot.HeroId || Entry->OwnerKind != Snapshot.OwnerKind
					|| Entry->Controller != Snapshot.Controller.Get()
					|| Snapshot.Knight->GetKnightEntityId() != Snapshot.KnightId
					|| Snapshot.Knight->GetHeroDefinitionId() != Snapshot.HeroId
					|| Snapshot.Knight->GetController() != Snapshot.Controller.Get()
					|| Snapshot.Controller->GetPawn() != Snapshot.Knight.Get())
				{
					Fail(TEXT("Boundary movement/fall replaced an original Knight, identity, definition or controller."));
					return false;
				}
			}
			return true;
		}

		void WalkIntoWall()
		{
			const FVector Position = ControlledKnight->GetActorLocation();
			if (Position.X > ExpectedWallStopX + WallDriftToleranceCm)
			{
				Fail(TEXT("Mapped forward movement penetrated the authored east wall."));
				return;
			}
			if (Position.X >= ExpectedWallStopX - WallContactToleranceCm
				&& FMath::Abs(ControlledKnight->GetVelocity().X) < WallDriftToleranceCm)
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
				EnterPhase(EKnightBoundaryPhase::HoldWall, WallTimeoutSeconds);
			}
		}

		void HoldAgainstWall()
		{
			const FVector Position = ControlledKnight->GetActorLocation();
			if (Position.X > ExpectedWallStopX + WallDriftToleranceCm
				|| FMath::Abs(Position.X - WallContactX) > WallDriftToleranceCm
				|| !ControlledKnight->GetCharacterMovement()->IsMovingOnGround())
			{
				Fail(TEXT("Three seconds of held W leaked through the wall or dropped the canonical Knight off the yard."));
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
			ControlledKnight->GetCharacterMovement()->StopMovementImmediately();
			if (!ControlledKnight->SetActorLocation(FallFaultLocation, false, nullptr, ETeleportType::TeleportPhysics))
			{
				Fail(TEXT("The explicit out-of-yard fault injection could not position the original canonical Knight."));
				return;
			}
			ControlledKnight->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			MinimumFallZ = FallFaultLocation.Z;
			Test->AddInfo(FString::Printf(
				TEXT("Fault injection: original actor=%s at %s; engine gravity must reach KillZ %.0f."),
				*ControlledKnight->GetPathName(), *FallFaultLocation.ToString(), World->GetWorldSettings()->KillZ));
			EnterPhase(EKnightBoundaryPhase::WaitRecovery, RecoveryTimeoutSeconds);
		}

		void WaitForRecovery()
		{
			const FVector Position = ControlledKnight->GetActorLocation();
			MinimumFallZ = FMath::Min(MinimumFallZ, Position.Z);
			bObservedGravity |= Position.Z < FallFaultLocation.Z - 100.0
				&& ControlledKnight->GetCharacterMovement()->IsFalling() && ControlledKnight->GetVelocity().Z < -50.0;
			AKnightPartyGameMode* Mode = World->GetAuthGameMode<AKnightPartyGameMode>();
			if (Mode == nullptr)
			{
				Fail(TEXT("The live party GameMode disappeared during the real KillZ regression."));
				return;
			}
			RecoverySpawn = Mode->GetSpawnLocationForSlot(ControlledSlot);
			if (FVector::Dist(Position, RecoverySpawn) > SpawnToleranceCm
				|| !ControlledKnight->GetCharacterMovement()->IsMovingOnGround())
			{
				return;
			}
			if (!bObservedGravity || MinimumFallZ > ExpectedKillZ + 500.0 || !ValidateCanonicalState())
			{
				Fail(TEXT("Spawn recovery was observed without a real gravity descent near KillZ or intact canonical state."));
				return;
			}
			const FKnightTrainingProgress& Progress = State->GetTrainingProgress();
			if (Progress.Revision != ProgressBeforeFall.Revision || Progress.MarkerMask != ProgressBeforeFall.MarkerMask
				|| Progress.ControlledMask != ProgressBeforeFall.ControlledMask
				|| Progress.bCompleted != ProgressBeforeFall.bCompleted)
			{
				Fail(TEXT("Automatic fall recovery reset training progression instead of preserving existing Knight state."));
				return;
			}
			Test->AddInfo(FString::Printf(
				TEXT("Real gravity/KillZ recovery: lowest sampled Z=%.2f, restored=%s, roster revision=%d."),
				MinimumFallZ, *Position.ToString(), Party->GetRosterRevision()));
			EnterPhase(EKnightBoundaryPhase::VerifyRecovery, RecoveryTimeoutSeconds);
		}

		void VerifyRecovery()
		{
			if (WorldElapsed() < SettleSeconds)
			{
				return;
			}
			if (!ControlledKnight->GetCharacterMovement()->IsMovingOnGround()
				|| FVector::Dist(ControlledKnight->GetActorLocation(), RecoverySpawn) > SpawnToleranceCm)
			{
				Fail(TEXT("The recovered canonical Knight did not remain safely walking at its authored spawn."));
				return;
			}
			if (!ValidateCanonicalState())
			{
				return;
			}
			Test->AddInfo(TEXT("Recovered Knight remained possessed and walking; canonical party state is unchanged."));
			EnterPhase(EKnightBoundaryPhase::StopPIE, CleanupTimeoutSeconds);
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
			Phase = EKnightBoundaryPhase::Complete;
		}

		FAutomationTestBase* Test;
		EKnightBoundaryPhase Phase = EKnightBoundaryPhase::WaitNavigation;
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
		FKnightTrainingProgress ProgressBeforeFall;
		FVector RecoverySpawn = FVector::ZeroVector;
		TArray<FKnightBoundarySnapshot> Snapshots;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AKnightGameState> State;
		TWeakObjectPtr<AKnightPartyState> Party;
		TWeakObjectPtr<AKnightPlayerController> Player;
		TWeakObjectPtr<AKnightCharacter> ControlledKnight;
		TStrongObjectPtr<ULevelEditorPlaySettings> PlaySettings;
		TUniquePtr<FStartPIEForAutomationCommand> StartCommand;
		FWaitForEditorNavMeshBuildCommand NavigationWait;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightPartyBoundaryTest, "Knight.Framework.PartyBoundaryRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightPartyBoundaryTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->PlayWorld != nullptr)
	{
		AddError(TEXT("Boundary regression requires an Editor without an existing PIE session."));
		return false;
	}
	if (!FPackageName::DoesPackageExist(KnightPartyBoundary::MapPath)
		|| !FEditorFileUtils::LoadMap(*KnightPartyBoundary::MapPath, false, false))
	{
		AddError(TEXT("The real training map is missing or cannot be loaded; rebuild and bootstrap M1 first."));
		return false;
	}
	AddCommand(new KnightPartyBoundary::FKnightPartyBoundaryCommand(this));
	return true;
}

#endif
