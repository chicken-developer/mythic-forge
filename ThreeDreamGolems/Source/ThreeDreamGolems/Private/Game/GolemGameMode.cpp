#include "Game/GolemGameMode.h"

#include "Character/GolemCharacter.h"
#include "Controllers/GolemCompanionController.h"
#include "Engine/World.h"
#include "Framework/GolemGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Logging/GolemLog.h"
#include "Party/GolemPartyState.h"
#include "Player/GolemPlayerController.h"
#include "Training/GolemTrainingHUD.h"

#include "AIController.h"

AGolemGameMode::AGolemGameMode()
{
	GolemCharacterClass = AGolemCharacter::StaticClass();
	AIControllerClass = AGolemCompanionController::StaticClass();
	PlayerControllerClass = AGolemPlayerController::StaticClass();
	HUDClass = AGolemTrainingHUD::StaticClass();
	SpawnOrigin = FVector(0.0, -900.0, 100.0);
	SlotSpacing = 120.0f;
}

FGolemResult AGolemGameMode::TryRestartDrill(APlayerController* Player)
{
	AGolemPartyState* Party = GetPartyState();
	AGolemGameState* State = GetGameState<AGolemGameState>();
	if (!HasAuthority() || !IsValid(Player) || Player->GetWorld() != GetWorld())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("RestartDrill"),
			NSLOCTEXT("KnightTraining", "RetryAuthority", "Only the host may reset the training yard."));
	}
	if (!Player->IsLocalController() || Party == nullptr || State == nullptr
		|| Party->GetControlledSlot(Player) == INDEX_NONE || !Party->IsCanonicalRosterReady())
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("RestartDrill"),
			NSLOCTEXT("KnightTraining", "RetryHost", "The host must control a ready canonical Golem to restart."));
	}
	for (const FGolemRosterEntry& Entry : Party->GetRoster())
	{
		if (!Entry.Golem->CanTransferControl())
		{
			return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RestartDrill"),
				NSLOCTEXT("KnightTraining", "RetryBusy", "A Golem's current gameplay state blocks restarting."));
		}
	}
	for (const FGolemRosterEntry& Entry : Party->GetRoster())
	{
		AGolemCharacter* Golem = Entry.Golem;
		if (AAIController* Companion = Cast<AAIController>(Golem->GetController()))
		{
			Companion->StopMovement();
		}
		if (AGolemPlayerController* Human = Cast<AGolemPlayerController>(Golem->GetController()))
		{
			Human->ResetControlInput();
			Human->SetControlRotation(FRotator::ZeroRotator);
		}
		Golem->ResetControlInput();
		Golem->GetCharacterMovement()->StopMovementImmediately();
		Golem->SetActorLocationAndRotation(GetSpawnLocationForSlot(Entry.Slot), FRotator::ZeroRotator,
			false, nullptr, ETeleportType::TeleportPhysics);
		Golem->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Golem->ForceNetUpdate();
	}
	const FGolemResult Reset = State->ResetTrainingProgress(Player);
	if (!Reset.IsSuccess())
	{
		return Reset;
	}
	for (const FGolemRosterEntry& Entry : Party->GetRoster())
	{
		if (APlayerController* Human = Cast<APlayerController>(Entry.Golem->GetController()))
		{
			const FGolemResult Record = State->RecordControlledSlot(Human, Entry.Slot);
			if (!Record.IsSuccess())
			{
				return Record;
			}
		}
	}
	UE_LOG(LogGolemCore, Display, TEXT("Training drill reset; actors/IDs preserved; canonical count %d"),
		Party->GetRoster().Num());
	return FGolemResult::Success();
}
