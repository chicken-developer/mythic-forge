#include "GearLab/KnightGearLabState.h"
#include "Net/UnrealNetwork.h"

AKnightGearLabState::AKnightGearLabState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
}

void AKnightGearLabState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKnightGearLabState, Snapshot);
}

const FKnightGearDemoSnapshot& AKnightGearLabState::GetSnapshot() const
{
	return Snapshot;
}

void AKnightGearLabState::PublishFixture(const FKnightGearDemoSnapshot& InSnapshot)
{
	if (HasAuthority())
	{
		Snapshot = InSnapshot;
		++Snapshot.Revision;
		ForceNetUpdate();
		OnRep_Snapshot();
	}
}

void AKnightGearLabState::OnRep_Snapshot()
{
	OnGearDemoChanged.Broadcast(Snapshot);
}
