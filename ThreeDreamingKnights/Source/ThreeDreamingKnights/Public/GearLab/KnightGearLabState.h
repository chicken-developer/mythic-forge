#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KnightGearLabState.generated.h"

USTRUCT(BlueprintType)
struct THREEDREAMINGKNIGHTS_API FKnightGearDemoSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Gear|Fixture")
	FGuid DemoRunId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Gear|Fixture")
	bool bCatalogLab = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Gear|Fixture")
	int32 CompletedEncounters = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Gear|Fixture")
	FGuid ClaimableRewardId;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Gear|Fixture")
	FText StageLabel;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Gear|Fixture")
	int64 Revision = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightGearDemoChanged, const FKnightGearDemoSnapshot&);

/** Presentation of the isolated fixture, never campaign/quest authority. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightGearLabState : public AActor
{
	GENERATED_BODY()
public:
	AKnightGearLabState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	const FKnightGearDemoSnapshot& GetSnapshot() const;
	void PublishFixture(const FKnightGearDemoSnapshot& InSnapshot);
	FOnKnightGearDemoChanged OnGearDemoChanged;
private:
	UFUNCTION()
	void OnRep_Snapshot();
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FKnightGearDemoSnapshot Snapshot;
};
