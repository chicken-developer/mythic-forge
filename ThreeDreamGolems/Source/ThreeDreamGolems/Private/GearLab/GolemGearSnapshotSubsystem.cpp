#include "GearLab/GolemGearSnapshotSubsystem.h"

#include "Character/GolemCharacter.h"
#include "Components/GolemCombatComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"

bool UGolemGearSnapshotSubsystem::HasPendingReconstruction() const
{
	return bPending;
}

FGolemResult UGolemGearSnapshotSubsystem::StageReconstruction(const FGolemRPGSnapshot& InSnapshot,
	const FGolemGearDemoSnapshot& InFixture)
{
	if (bPending || !InSnapshot.View.bGearDemo || !InFixture.DemoRunId.IsValid()
		|| InSnapshot.View.DemoRunId != InFixture.DemoRunId || InSnapshot.View.Golems.Num() != 3)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("GearFixtureSnapshot"), FText::GetEmpty());
	}
	TMap<FGolemEntityId, float> CapturedPoise;
	for (const FGolemRPGGolemView& Golem : InSnapshot.View.Golems)
	{
		for (TActorIterator<AGolemCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetGolemEntityId() == Golem.GolemId && It->GetCombatComponent() != nullptr)
			{
				CapturedPoise.Add(Golem.GolemId, It->GetCombatComponent()->GetSnapshot().Poise);
				break;
			}
		}
	}
	if (CapturedPoise.Num() != 3)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("GearSnapshotPoise"), FText::GetEmpty());
	}
	Snapshot = InSnapshot;
	Fixture = InFixture;
	PoisePools = MoveTemp(CapturedPoise);
	CapturedRealTime = FPlatformTime::Seconds();
	bPending = true;
	return FGolemResult::Success();
}

FGolemRPGSnapshot UGolemGearSnapshotSubsystem::GetElapsedSnapshot() const
{
	FGolemRPGSnapshot Result = Snapshot;
	const float Elapsed = static_cast<float>(FMath::Max(0.0, FPlatformTime::Seconds() - CapturedRealTime));
	for (TPair<FGolemEntityId, FGolemRPGCombatSnapshot>& Entry : Result.CombatStates)
	{
		FGolemSkillRuntimeSnapshot& Runtime = Entry.Value.Skills;
		for (float& Remaining : Runtime.CooldownRemaining)
		{
			Remaining = FMath::Max(0.0f, Remaining - Elapsed);
		}
		float* Durations[]{&Runtime.StackRemaining, &Runtime.BuffRemaining, &Runtime.WaterReadyRemaining,
			&Runtime.IceReadyRemaining, &Runtime.AftershockRemaining, &Runtime.StackGainRemaining,
			&Runtime.ResourceRegenDelayRemaining, &Runtime.BulwarkRemaining, &Runtime.LifeseedRemaining,
			&Runtime.StarfallRemaining, &Runtime.StarfallCounterRemaining, &Runtime.ConsumableRemaining};
		for (float* Remaining : Durations)
		{
			*Remaining = FMath::Max(0.0f, *Remaining - Elapsed);
		}
		if (Runtime.StackRemaining == 0.0f)
		{
			Runtime.Stacks = 0;
		}
		if (Runtime.StarfallCounterRemaining == 0.0f)
		{
			Runtime.StarfallCount = 0;
		}
	}
	return Result;
}

const FGolemGearDemoSnapshot& UGolemGearSnapshotSubsystem::GetFixtureSnapshot() const
{
	return Fixture;
}

FGolemEntityId UGolemGearSnapshotSubsystem::FindCanonicalIdentity(const FPrimaryAssetId& HeroId) const
{
	const FGolemRPGGolemView* Golem = Snapshot.View.Golems.FindByPredicate([&HeroId](const FGolemRPGGolemView& Entry)
	{
		return Entry.HeroDefinitionId == HeroId;
	});
	return bPending && Golem != nullptr ? Golem->GolemId : FGolemEntityId();
}

float UGolemGearSnapshotSubsystem::GetCapturedPoise(const FGolemEntityId& GolemId) const
{
	const float* Pool = PoisePools.Find(GolemId);
	return bPending && Pool != nullptr ? *Pool : -1.0f;
}

void UGolemGearSnapshotSubsystem::ConsumeReconstruction()
{
	bPending = false;
	Snapshot = FGolemRPGSnapshot();
	Fixture = FGolemGearDemoSnapshot();
	PoisePools.Reset();
}
