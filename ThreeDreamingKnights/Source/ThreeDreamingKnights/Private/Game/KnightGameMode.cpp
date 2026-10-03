#include "Game/KnightGameMode.h"

#include "Character/KnightCharacter.h"
#include "Controllers/KnightCompanionController.h"
#include "Engine/World.h"
#include "Framework/KnightGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Logging/KnightLog.h"
#include "Party/KnightPartyState.h"
#include "Player/KnightPlayerController.h"
#include "Training/KnightTrainingHUD.h"

#include "AIController.h"

AKnightGameMode::AKnightGameMode()
{
	KnightCharacterClass = AKnightCharacter::StaticClass();
	AIControllerClass = AKnightCompanionController::StaticClass();
	PlayerControllerClass = AKnightPlayerController::StaticClass();
	HUDClass = AKnightTrainingHUD::StaticClass();
	SpawnOrigin = FVector(0.0, -900.0, 100.0);
	SlotSpacing = 120.0f;
}

FKnightResult AKnightGameMode::TryRestartDrill(APlayerController* Player)
{
	AKnightPartyState* Party = GetPartyState();
	AKnightGameState* State = GetGameState<AKnightGameState>();
	if (!HasAuthority() || !IsValid(Player) || Player->GetWorld() != GetWorld())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("RestartDrill"),
			NSLOCTEXT("KnightTraining", "RetryAuthority", "Only the host may reset the training yard."));
	}
	if (!Player->IsLocalController() || Party == nullptr || State == nullptr
		|| Party->GetControlledSlot(Player) == INDEX_NONE || !Party->IsCanonicalRosterReady())
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("RestartDrill"),
			NSLOCTEXT("KnightTraining", "RetryHost", "The host must control a ready canonical Knight to restart."));
	}
	for (const FKnightRosterEntry& Entry : Party->GetRoster())
	{
		if (!Entry.Knight->CanTransferControl())
		{
			return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RestartDrill"),
				NSLOCTEXT("KnightTraining", "RetryBusy", "A Knight's current gameplay state blocks restarting."));
		}
	}
	for (const FKnightRosterEntry& Entry : Party->GetRoster())
	{
		AKnightCharacter* Knight = Entry.Knight;
		if (AAIController* Companion = Cast<AAIController>(Knight->GetController()))
		{
			Companion->StopMovement();
		}
		if (AKnightPlayerController* Human = Cast<AKnightPlayerController>(Knight->GetController()))
		{
			Human->ResetControlInput();
			Human->SetControlRotation(FRotator::ZeroRotator);
		}
		Knight->ResetControlInput();
		Knight->GetCharacterMovement()->StopMovementImmediately();
		Knight->SetActorLocationAndRotation(GetSpawnLocationForSlot(Entry.Slot), FRotator::ZeroRotator,
			false, nullptr, ETeleportType::TeleportPhysics);
		Knight->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Knight->ForceNetUpdate();
	}
	const FKnightResult Reset = State->ResetTrainingProgress(Player);
	if (!Reset.IsSuccess())
	{
		return Reset;
	}
	for (const FKnightRosterEntry& Entry : Party->GetRoster())
	{
		if (APlayerController* Human = Cast<APlayerController>(Entry.Knight->GetController()))
		{
			const FKnightResult Record = State->RecordControlledSlot(Human, Entry.Slot);
			if (!Record.IsSuccess())
			{
				return Record;
			}
		}
	}
	UE_LOG(LogKnightCore, Display, TEXT("Training drill reset; actors/IDs preserved; canonical count %d"),
		Party->GetRoster().Num());
	return FKnightResult::Success();
}
