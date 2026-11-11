#include "GearLab/GolemGearLabState.h"
#include "Net/UnrealNetwork.h"

AGolemGearLabState::AGolemGearLabState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
}

void AGolemGearLabState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemGearLabState, Snapshot);
}

const FGolemGearDemoSnapshot& AGolemGearLabState::GetSnapshot() const
{
	return Snapshot;
}

void AGolemGearLabState::PublishFixture(const FGolemGearDemoSnapshot& InSnapshot)
{
	if (HasAuthority())
	{
		Snapshot = InSnapshot;
		++Snapshot.Revision;
		ForceNetUpdate();
		OnRep_Snapshot();
	}
}

void AGolemGearLabState::OnRep_Snapshot()
{
	OnGearDemoChanged.Broadcast(Snapshot);
}
