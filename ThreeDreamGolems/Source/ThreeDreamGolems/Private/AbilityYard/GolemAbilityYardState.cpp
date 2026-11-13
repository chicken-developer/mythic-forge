#include "AbilityYard/GolemAbilityYardState.h"

#include "AbilityYard/GolemAbilityDummy.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AGolemAbilityYardState::AGolemAbilityYardState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.0f);
}

void AGolemAbilityYardState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemAbilityYardState, Snapshot);
	DOREPLIFETIME(AGolemAbilityYardState, Dummy);
}

AGolemAbilityYardState* AGolemAbilityYardState::Find(UWorld* World)
{
	if (World != nullptr)
	{
		for (TActorIterator<AGolemAbilityYardState> Iterator(World); Iterator; ++Iterator)
		{
			return *Iterator;
		}
	}
	return nullptr;
}

const FGolemAbilityYardSnapshot& AGolemAbilityYardState::GetSnapshot() const
{
	return Snapshot;
}

AGolemAbilityDummy* AGolemAbilityYardState::GetDummy() const
{
	return Dummy;
}

void AGolemAbilityYardState::CommitSnapshot()
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

void AGolemAbilityYardState::OnRep_Snapshot()
{
	OnSnapshotChanged.Broadcast();
}
