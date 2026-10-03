#include "Framework/KnightGameState.h"

#include "Character/KnightCharacter.h"
#include "Party/KnightPartyState.h"
#include "Player/KnightPlayerState.h"
#include "Settings/KnightSettings.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "KnightGameState"

void AKnightGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKnightGameState, PartyState);
	DOREPLIFETIME(AKnightGameState, TrainingProgress);
}

AKnightPartyState* AKnightGameState::GetPartyState() const
{
	return PartyState;
}

const FKnightTrainingProgress& AKnightGameState::GetTrainingProgress() const
{
	return TrainingProgress;
}

FKnightResult AKnightGameState::SetPartyState(AKnightPartyState* InPartyState)
{
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("PartyState"),
			LOCTEXT("PartyStateAuthority", "Only the server may bind canonical party state."));
	}
	if (!IsValid(InPartyState) || InPartyState->GetWorld() != GetWorld()
		|| !InPartyState->IsCanonicalRosterReady())
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("PartyState"),
			LOCTEXT("PartyStateInvalid", "Party state must contain the complete canonical roster in this world."));
	}
	if (PartyState != nullptr && PartyState != InPartyState)
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("PartyState"),
			LOCTEXT("PartyStateAlreadyBound", "The world's canonical party cannot be replaced."));
	}
	if (PartyState == nullptr)
	{
		PartyState = InPartyState;
		OnPartyStateChanged.Broadcast(PartyState);
		ForceNetUpdate();
	}
	return FKnightResult::Success();
}

FKnightResult AKnightGameState::RecordControlledSlot(APlayerController* Player, int32 Slot)
{
	const FKnightResult Validation = ValidateHumanCommand(Player);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (Slot < 0 || Slot >= UKnightSettings::CanonicalPartySize
		|| PartyState->GetControlledSlot(Player) != Slot)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("TrainingControl"),
			LOCTEXT("TrainingControlOwnership", "The human must actually control the Knight being recorded."));
	}
	const uint8 SlotBit = static_cast<uint8>(1 << Slot);
	if ((TrainingProgress.ControlledMask & SlotBit) == 0)
	{
		TrainingProgress.ControlledMask |= SlotBit;
		CommitTrainingProgress();
	}
	return FKnightResult::Success();
}

FKnightResult AKnightGameState::RecordTrainingMarker(APlayerController* Player, int32 MarkerIndex)
{
	const FKnightResult Validation = ValidateHumanCommand(Player);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (MarkerIndex < 0 || MarkerIndex >= RequiredMarkerCount)
	{
		return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("TrainingMarker"),
			LOCTEXT("TrainingMarkerRange", "The requested training marker does not exist."));
	}
	const uint8 MarkerBit = static_cast<uint8>(1 << MarkerIndex);
	if ((TrainingProgress.MarkerMask & MarkerBit) == 0)
	{
		TrainingProgress.MarkerMask |= MarkerBit;
		CommitTrainingProgress();
	}
	return FKnightResult::Success();
}

FKnightResult AKnightGameState::ResetTrainingProgress(APlayerController* Player)
{
	const FKnightResult Validation = ValidateHumanCommand(Player);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	TrainingProgress.ControlledMask = 0;
	TrainingProgress.MarkerMask = 0;
	TrainingProgress.bCompleted = false;
	const int32 CurrentSlot = PartyState->GetControlledSlot(Player);
	TrainingProgress.ControlledMask = static_cast<uint8>(1 << CurrentSlot);
	CommitTrainingProgress();
	return FKnightResult::Success();
}

FKnightResult AKnightGameState::ValidateHumanCommand(const APlayerController* Player) const
{
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("Training"),
			LOCTEXT("TrainingAuthority", "Only the server may change training progress."));
	}
	if (!IsValid(Player) || Player->GetWorld() != GetWorld()
		|| PartyState == nullptr || !PartyState->IsCanonicalRosterReady())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("Training"),
			LOCTEXT("TrainingNotReady", "A valid human controller and canonical party are required."));
	}
	const int32 Slot = PartyState->GetControlledSlot(Player);
	const FKnightRosterEntry* Entry = PartyState->FindSlot(Slot);
	if (Entry == nullptr || Entry->OwnerKind != EKnightControllerOwner::Human
		|| Entry->Knight != Player->GetPawn() || !IsValid(Entry->HumanPlayerState))
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("Training"),
			LOCTEXT("TrainingHumanOwnership", "Training commands require ownership of a canonical Knight."));
	}
	return FKnightResult::Success();
}

void AKnightGameState::CommitTrainingProgress()
{
	const uint8 RequiredControlMask = static_cast<uint8>((1 << UKnightSettings::CanonicalPartySize) - 1);
	const uint8 RequiredMarkerMask = static_cast<uint8>((1 << RequiredMarkerCount) - 1);
	TrainingProgress.bCompleted = TrainingProgress.ControlledMask == RequiredControlMask
		&& TrainingProgress.MarkerMask == RequiredMarkerMask;
	++TrainingProgress.Revision;
	ForceNetUpdate();
	OnTrainingProgressChanged.Broadcast(TrainingProgress);
}

void AKnightGameState::OnRep_PartyState()
{
	OnPartyStateChanged.Broadcast(PartyState);
}

void AKnightGameState::OnRep_TrainingProgress()
{
	OnTrainingProgressChanged.Broadcast(TrainingProgress);
}

#undef LOCTEXT_NAMESPACE
