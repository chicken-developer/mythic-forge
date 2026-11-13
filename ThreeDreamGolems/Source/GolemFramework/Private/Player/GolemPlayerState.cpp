#include "Player/GolemPlayerState.h"

#include "Settings/GolemSettings.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "GolemPlayerState"

void AGolemPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemPlayerState, HumanId);
	DOREPLIFETIME(AGolemPlayerState, AssignedSlot);
}

FGuid AGolemPlayerState::GetHumanId() const
{
	return HumanId;
}

int32 AGolemPlayerState::GetAssignedSlot() const
{
	return AssignedSlot;
}

FGolemResult AGolemPlayerState::InitializeHumanIdentity()
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("HumanIdentity"),
			LOCTEXT("IdentityAuthority", "Only the server may initialize human connection identity."));
	}
	if (!HumanId.IsValid())
	{
		HumanId = FGuid::NewGuid();
		ForceNetUpdate();
	}
	return FGolemResult::Success();
}

FGolemResult AGolemPlayerState::SetAssignedSlot(int32 InAssignedSlot)
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("HumanAssignment"),
			LOCTEXT("AssignmentAuthority", "Only the server may change a human's party slot."));
	}
	if (InAssignedSlot < INDEX_NONE || InAssignedSlot >= UGolemSettings::CanonicalPartySize)
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("HumanAssignment"),
			LOCTEXT("AssignmentRange", "The requested human slot is outside the canonical party."));
	}
	InitializeHumanIdentity();
	if (AssignedSlot != InAssignedSlot)
	{
		AssignedSlot = InAssignedSlot;
		OnAssignmentChanged.Broadcast(AssignedSlot);
		ForceNetUpdate();
	}
	return FGolemResult::Success();
}

void AGolemPlayerState::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		InitializeHumanIdentity();
	}
}

void AGolemPlayerState::OnRep_AssignedSlot()
{
	OnAssignmentChanged.Broadcast(AssignedSlot);
}

#undef LOCTEXT_NAMESPACE
