#include "GearLab/KnightGearSnapshotSubsystem.h"

#include "Character/KnightCharacter.h"
#include "Components/KnightCombatComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"

bool UKnightGearSnapshotSubsystem::HasPendingReconstruction() const
{
	return bPending;
}

FKnightResult UKnightGearSnapshotSubsystem::StageReconstruction(const FKnightRPGSnapshot& InSnapshot,
	const FKnightGearDemoSnapshot& InFixture)
{
	if (bPending || !InSnapshot.View.bGearDemo || !InFixture.DemoRunId.IsValid()
		|| InSnapshot.View.DemoRunId != InFixture.DemoRunId || InSnapshot.View.Knights.Num() != 3)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("GearFixtureSnapshot"), FText::GetEmpty());
	}
	TMap<FKnightEntityId, float> CapturedPoise;
	for (const FKnightRPGKnightView& Knight : InSnapshot.View.Knights)
	{
		for (TActorIterator<AKnightCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetKnightEntityId() == Knight.KnightId && It->GetCombatComponent() != nullptr)
			{
				CapturedPoise.Add(Knight.KnightId, It->GetCombatComponent()->GetSnapshot().Poise);
				break;
			}
		}
	}
	if (CapturedPoise.Num() != 3)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("GearSnapshotPoise"), FText::GetEmpty());
	}
	Snapshot = InSnapshot;
	Fixture = InFixture;
	PoisePools = MoveTemp(CapturedPoise);
	CapturedRealTime = FPlatformTime::Seconds();
	bPending = true;
	return FKnightResult::Success();
}

FKnightRPGSnapshot UKnightGearSnapshotSubsystem::GetElapsedSnapshot() const
{
	FKnightRPGSnapshot Result = Snapshot;
	const float Elapsed = static_cast<float>(FMath::Max(0.0, FPlatformTime::Seconds() - CapturedRealTime));
	for (TPair<FKnightEntityId, FKnightRPGCombatSnapshot>& Entry : Result.CombatStates)
	{
		FKnightSkillRuntimeSnapshot& Runtime = Entry.Value.Skills;
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

const FKnightGearDemoSnapshot& UKnightGearSnapshotSubsystem::GetFixtureSnapshot() const
{
	return Fixture;
}

FKnightEntityId UKnightGearSnapshotSubsystem::FindCanonicalIdentity(const FPrimaryAssetId& HeroId) const
{
	const FKnightRPGKnightView* Knight = Snapshot.View.Knights.FindByPredicate([&HeroId](const FKnightRPGKnightView& Entry)
	{
		return Entry.HeroDefinitionId == HeroId;
	});
	return bPending && Knight != nullptr ? Knight->KnightId : FKnightEntityId();
}

float UKnightGearSnapshotSubsystem::GetCapturedPoise(const FKnightEntityId& KnightId) const
{
	const float* Pool = PoisePools.Find(KnightId);
	return bPending && Pool != nullptr ? *Pool : -1.0f;
}

void UKnightGearSnapshotSubsystem::ConsumeReconstruction()
{
	bPending = false;
	Snapshot = FKnightRPGSnapshot();
	Fixture = FKnightGearDemoSnapshot();
	PoisePools.Reset();
}
