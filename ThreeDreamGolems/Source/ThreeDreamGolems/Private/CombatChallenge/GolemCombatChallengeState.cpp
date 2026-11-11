#include "CombatChallenge/GolemCombatChallengeState.h"

#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AGolemCombatChallengeState::AGolemCombatChallengeState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(10.0f);
}

void AGolemCombatChallengeState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGolemCombatChallengeState, Snapshot);
}

AGolemCombatChallengeState* AGolemCombatChallengeState::Find(UWorld* World)
{
	if (World != nullptr)
	{
		for (TActorIterator<AGolemCombatChallengeState> Iterator(World); Iterator; ++Iterator)
		{
			return *Iterator;
		}
	}
	return nullptr;
}

const FGolemCombatChallengeSnapshot& AGolemCombatChallengeState::GetSnapshot() const
{
	return Snapshot;
}

void AGolemCombatChallengeState::CommitSnapshot()
{
	if (!HasAuthority())
	{
		return;
	}
	++Snapshot.Revision;
	Snapshot.bCompleted = !Snapshot.bFailed && Snapshot.LightHitCount > 0 && Snapshot.HeavyHitCount > 0
		&& Snapshot.ComboCount > 0 && Snapshot.BlockCount > 0 && Snapshot.ParryCount > 0
		&& Snapshot.DodgeCount > 0 && Snapshot.StaggerCount > 0 && Snapshot.RuneHitCount > 0
		&& Snapshot.FinisherCount > 0 && Snapshot.TraversalCount > 0 && Snapshot.bTargetLocked;
	ForceNetUpdate();
	OnSnapshotChanged.Broadcast();
}

void AGolemCombatChallengeState::OnRep_Snapshot()
{
	OnSnapshotChanged.Broadcast();
}
