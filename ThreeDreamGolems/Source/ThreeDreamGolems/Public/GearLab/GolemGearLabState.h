#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GolemGearLabState.generated.h"

USTRUCT(BlueprintType)
struct THREEDREAMGOLEMS_API FGolemGearDemoSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Gear|Fixture")
	FGuid DemoRunId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Gear|Fixture")
	bool bCatalogLab = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Gear|Fixture")
	int32 CompletedEncounters = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Gear|Fixture")
	FGuid ClaimableRewardId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Gear|Fixture")
	FText StageLabel;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Gear|Fixture")
	int64 Revision = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemGearDemoChanged, const FGolemGearDemoSnapshot&);

/** Presentation of the isolated fixture, never campaign/quest authority. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemGearLabState : public AActor
{
	GENERATED_BODY()
public:
	AGolemGearLabState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	const FGolemGearDemoSnapshot& GetSnapshot() const;
	void PublishFixture(const FGolemGearDemoSnapshot& InSnapshot);
	FOnGolemGearDemoChanged OnGearDemoChanged;
private:
	UFUNCTION()
	void OnRep_Snapshot();
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FGolemGearDemoSnapshot Snapshot;
};
