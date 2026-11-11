#include "PartyArena/GolemPartyArenaState.h"

#include "Net/UnrealNetwork.h"

AGolemPartyArenaState::AGolemPartyArenaState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
}

void AGolemPartyArenaState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemPartyArenaState, Snapshot);
}

const FGolemPartyArenaSnapshot& AGolemPartyArenaState::GetSnapshot() const
{
	return Snapshot;
}

void AGolemPartyArenaState::CommitSnapshot(const FGolemPartyArenaSnapshot& Candidate)
{
	if (!HasAuthority() || IsActorBeingDestroyed())
	{
		return;
	}
	const int32 PreviousRevision = Snapshot.Revision;
	Snapshot = Candidate;
	Snapshot.Revision = PreviousRevision + 1;
	ForceNetUpdate();
	OnSnapshotChanged.Broadcast(Snapshot);
}

void AGolemPartyArenaState::OnRep_Snapshot()
{
	OnSnapshotChanged.Broadcast(Snapshot);
}
