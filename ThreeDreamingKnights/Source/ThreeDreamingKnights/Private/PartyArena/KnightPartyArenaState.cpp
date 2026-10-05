#include "PartyArena/KnightPartyArenaState.h"

#include "Net/UnrealNetwork.h"

AKnightPartyArenaState::AKnightPartyArenaState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
}

void AKnightPartyArenaState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKnightPartyArenaState, Snapshot);
}

const FKnightPartyArenaSnapshot& AKnightPartyArenaState::GetSnapshot() const
{
	return Snapshot;
}

void AKnightPartyArenaState::CommitSnapshot(const FKnightPartyArenaSnapshot& Candidate)
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

void AKnightPartyArenaState::OnRep_Snapshot()
{
	OnSnapshotChanged.Broadcast(Snapshot);
}
