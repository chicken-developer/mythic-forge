#include "Player/KnightPlayerState.h"

#include "Settings/KnightSettings.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "KnightPlayerState"

void AKnightPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKnightPlayerState, HumanId);
	DOREPLIFETIME(AKnightPlayerState, AssignedSlot);
}

FGuid AKnightPlayerState::GetHumanId() const
{
	return HumanId;
}

int32 AKnightPlayerState::GetAssignedSlot() const
{
	return AssignedSlot;
}

FKnightResult AKnightPlayerState::InitializeHumanIdentity()
{
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("HumanIdentity"),
			LOCTEXT("IdentityAuthority", "Only the server may initialize human connection identity."));
	}
	if (!HumanId.IsValid())
	{
		HumanId = FGuid::NewGuid();
		ForceNetUpdate();
	}
	return FKnightResult::Success();
}

FKnightResult AKnightPlayerState::SetAssignedSlot(int32 InAssignedSlot)
{
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("HumanAssignment"),
			LOCTEXT("AssignmentAuthority", "Only the server may change a human's party slot."));
	}
	if (InAssignedSlot < INDEX_NONE || InAssignedSlot >= UKnightSettings::CanonicalPartySize)
	{
		return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("HumanAssignment"),
			LOCTEXT("AssignmentRange", "The requested human slot is outside the canonical party."));
	}
	InitializeHumanIdentity();
	if (AssignedSlot != InAssignedSlot)
	{
		AssignedSlot = InAssignedSlot;
		OnAssignmentChanged.Broadcast(AssignedSlot);
		ForceNetUpdate();
	}
	return FKnightResult::Success();
}

void AKnightPlayerState::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		InitializeHumanIdentity();
	}
}

void AKnightPlayerState::OnRep_AssignedSlot()
{
	OnAssignmentChanged.Broadcast(AssignedSlot);
}

#undef LOCTEXT_NAMESPACE
