#pragma once

#include "CoreMinimal.h"
#include "GearLab/KnightGearLabState.h"
#include "Models/KnightRPGModels.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "KnightGearSnapshotSubsystem.generated.h"

/** Memory-only isolated demo adapter. No disk format, offline policy or campaign persistence is implemented here. */
UCLASS()
class THREEDREAMINGKNIGHTS_API UKnightGearSnapshotSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	bool HasPendingReconstruction() const;
	FKnightResult StageReconstruction(const FKnightRPGSnapshot& InSnapshot, const FKnightGearDemoSnapshot& InFixture);
	FKnightRPGSnapshot GetElapsedSnapshot() const;
	const FKnightGearDemoSnapshot& GetFixtureSnapshot() const;
	FKnightEntityId FindCanonicalIdentity(const FPrimaryAssetId& HeroId) const;
	float GetCapturedPoise(const FKnightEntityId& KnightId) const;
	void ConsumeReconstruction();
private:
	UPROPERTY(Transient)
	FKnightRPGSnapshot Snapshot;
	UPROPERTY(Transient)
	FKnightGearDemoSnapshot Fixture;
	/** Combat owns this pool; keep the fixture fragment above Items rather than adding a reverse module dependency. */
	UPROPERTY(Transient)
	TMap<FKnightEntityId, float> PoisePools;
	double CapturedRealTime = 0.0;
	bool bPending = false;
};
