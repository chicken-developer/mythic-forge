#include "Framework/GolemGameState.h"

#include "Character/GolemCharacter.h"
#include "Party/GolemPartyState.h"
#include "Player/GolemPlayerState.h"
#include "Settings/GolemSettings.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "GolemGameState"

void AGolemGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemGameState, PartyState);
	DOREPLIFETIME(AGolemGameState, TrainingProgress);
}

AGolemPartyState* AGolemGameState::GetPartyState() const
{
	return PartyState;
}

const FGolemTrainingProgress& AGolemGameState::GetTrainingProgress() const
{
	return TrainingProgress;
}

FGolemResult AGolemGameState::SetPartyState(AGolemPartyState* InPartyState)
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("PartyState"),
			LOCTEXT("PartyStateAuthority", "Only the server may bind canonical party state."));
	}
	if (!IsValid(InPartyState) || InPartyState->GetWorld() != GetWorld()
		|| !InPartyState->IsCanonicalRosterReady())
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("PartyState"),
			LOCTEXT("PartyStateInvalid", "Party state must contain the complete canonical roster in this world."));
	}
	if (PartyState != nullptr && PartyState != InPartyState)
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("PartyState"),
			LOCTEXT("PartyStateAlreadyBound", "The world's canonical party cannot be replaced."));
	}
	if (PartyState == nullptr)
	{
		PartyState = InPartyState;
		OnPartyStateChanged.Broadcast(PartyState);
		ForceNetUpdate();
	}
	return FGolemResult::Success();
}

FGolemResult AGolemGameState::RecordControlledSlot(APlayerController* Player, int32 Slot)
{
	const FGolemResult Validation = ValidateHumanCommand(Player);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (Slot < 0 || Slot >= UGolemSettings::CanonicalPartySize
		|| PartyState->GetControlledSlot(Player) != Slot)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("TrainingControl"),
			LOCTEXT("TrainingControlOwnership", "The human must actually control the Golem being recorded."));
	}
	const uint8 SlotBit = static_cast<uint8>(1 << Slot);
	if ((TrainingProgress.ControlledMask & SlotBit) == 0)
	{
		TrainingProgress.ControlledMask |= SlotBit;
		CommitTrainingProgress();
	}
	return FGolemResult::Success();
}

FGolemResult AGolemGameState::RecordTrainingMarker(APlayerController* Player, int32 MarkerIndex)
{
	const FGolemResult Validation = ValidateHumanCommand(Player);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (MarkerIndex < 0 || MarkerIndex >= RequiredMarkerCount)
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("TrainingMarker"),
			LOCTEXT("TrainingMarkerRange", "The requested training marker does not exist."));
	}
	const uint8 MarkerBit = static_cast<uint8>(1 << MarkerIndex);
	if ((TrainingProgress.MarkerMask & MarkerBit) == 0)
	{
		TrainingProgress.MarkerMask |= MarkerBit;
		CommitTrainingProgress();
	}
	return FGolemResult::Success();
}

FGolemResult AGolemGameState::ResetTrainingProgress(APlayerController* Player)
{
	const FGolemResult Validation = ValidateHumanCommand(Player);
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
	return FGolemResult::Success();
}

FGolemResult AGolemGameState::ValidateHumanCommand(const APlayerController* Player) const
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("Training"),
			LOCTEXT("TrainingAuthority", "Only the server may change training progress."));
	}
	if (!IsValid(Player) || Player->GetWorld() != GetWorld()
		|| PartyState == nullptr || !PartyState->IsCanonicalRosterReady())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("Training"),
			LOCTEXT("TrainingNotReady", "A valid human controller and canonical party are required."));
	}
	const int32 Slot = PartyState->GetControlledSlot(Player);
	const FGolemRosterEntry* Entry = PartyState->FindSlot(Slot);
	if (Entry == nullptr || Entry->OwnerKind != EGolemControllerOwner::Human
		|| Entry->Golem != Player->GetPawn() || !IsValid(Entry->HumanPlayerState))
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("Training"),
			LOCTEXT("TrainingHumanOwnership", "Training commands require ownership of a canonical Golem."));
	}
	return FGolemResult::Success();
}

void AGolemGameState::CommitTrainingProgress()
{
	const uint8 RequiredControlMask = static_cast<uint8>((1 << UGolemSettings::CanonicalPartySize) - 1);
	const uint8 RequiredMarkerMask = static_cast<uint8>((1 << RequiredMarkerCount) - 1);
	TrainingProgress.bCompleted = TrainingProgress.ControlledMask == RequiredControlMask
		&& TrainingProgress.MarkerMask == RequiredMarkerMask;
	++TrainingProgress.Revision;
	ForceNetUpdate();
	OnTrainingProgressChanged.Broadcast(TrainingProgress);
}

void AGolemGameState::OnRep_PartyState()
{
	OnPartyStateChanged.Broadcast(PartyState);
}

void AGolemGameState::OnRep_TrainingProgress()
{
	OnTrainingProgressChanged.Broadcast(TrainingProgress);
}

#undef LOCTEXT_NAMESPACE
