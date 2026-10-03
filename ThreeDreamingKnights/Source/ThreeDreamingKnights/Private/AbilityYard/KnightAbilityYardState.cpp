#include "AbilityYard/KnightAbilityYardState.h"

#include "AbilityYard/KnightAbilityDummy.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AKnightAbilityYardState::AKnightAbilityYardState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.0f);
}

void AKnightAbilityYardState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKnightAbilityYardState, Snapshot);
	DOREPLIFETIME(AKnightAbilityYardState, Dummy);
}

AKnightAbilityYardState* AKnightAbilityYardState::Find(UWorld* World)
{
	if (World != nullptr)
	{
		for (TActorIterator<AKnightAbilityYardState> Iterator(World); Iterator; ++Iterator)
		{
			return *Iterator;
		}
	}
	return nullptr;
}

const FKnightAbilityYardSnapshot& AKnightAbilityYardState::GetSnapshot() const
{
	return Snapshot;
}

AKnightAbilityDummy* AKnightAbilityYardState::GetDummy() const
{
	return Dummy;
}

void AKnightAbilityYardState::CommitSnapshot()
{
	if (!HasAuthority())
	{
		return;
	}
	Snapshot.bCompleted = Snapshot.bDummyDamaged && Snapshot.bPlayerDamaged
		&& Snapshot.bCompanionDowned && Snapshot.bCompanionRevived;
	++Snapshot.Revision;
	ForceNetUpdate();
	OnSnapshotChanged.Broadcast();
}

void AKnightAbilityYardState::OnRep_Snapshot()
{
	OnSnapshotChanged.Broadcast();
}
