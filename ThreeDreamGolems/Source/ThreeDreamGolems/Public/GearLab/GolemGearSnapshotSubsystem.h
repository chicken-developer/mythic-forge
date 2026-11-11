#pragma once

#include "CoreMinimal.h"
#include "GearLab/GolemGearLabState.h"
#include "Models/GolemRPGModels.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GolemGearSnapshotSubsystem.generated.h"

/** Memory-only isolated demo adapter. No disk format, offline policy or campaign persistence is implemented here. */
UCLASS()
class THREEDREAMGOLEMS_API UGolemGearSnapshotSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	bool HasPendingReconstruction() const;
	FGolemResult StageReconstruction(const FGolemRPGSnapshot& InSnapshot, const FGolemGearDemoSnapshot& InFixture);
	FGolemRPGSnapshot GetElapsedSnapshot() const;
	const FGolemGearDemoSnapshot& GetFixtureSnapshot() const;
	FGolemEntityId FindCanonicalIdentity(const FPrimaryAssetId& HeroId) const;
	float GetCapturedPoise(const FGolemEntityId& GolemId) const;
	void ConsumeReconstruction();
private:
	UPROPERTY(Transient)
	FGolemRPGSnapshot Snapshot;
	UPROPERTY(Transient)
	FGolemGearDemoSnapshot Fixture;
	/** Combat owns this pool; keep the fixture fragment above Items rather than adding a reverse module dependency. */
	UPROPERTY(Transient)
	TMap<FGolemEntityId, float> PoisePools;
	double CapturedRealTime = 0.0;
	bool bPending = false;
};
