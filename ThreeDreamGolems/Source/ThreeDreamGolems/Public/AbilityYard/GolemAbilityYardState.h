#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/GolemStableIds.h"
#include "GolemAbilityYardState.generated.h"

class AGolemAbilityDummy;
class AGolemAbilityYardGameMode;

USTRUCT(BlueprintType)
struct THREEDREAMGOLEMS_API FGolemAbilityYardSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|AbilityYard")
	int32 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|AbilityYard")
	float DummyHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|AbilityYard")
	float DummyMaxHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|AbilityYard")
	bool bDummyDamaged = false;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|AbilityYard")
	bool bPlayerDamaged = false;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|AbilityYard")
	bool bCompanionDowned = false;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|AbilityYard")
	bool bCompanionRevived = false;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|AbilityYard")
	bool bCompleted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|AbilityYard")
	int32 ReviveSlot = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|AbilityYard")
	FGolemEntityId ReviveGolemId;
};

DECLARE_MULTICAST_DELEGATE(FOnGolemAbilityYardChanged);

/** Replicated exercise feedback observes real GAS results; presentation has no mutation API. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemAbilityYardState : public AActor
{
	GENERATED_BODY()

public:
	AGolemAbilityYardState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static AGolemAbilityYardState* Find(UWorld* World);
	const FGolemAbilityYardSnapshot& GetSnapshot() const;
	AGolemAbilityDummy* GetDummy() const;
	FOnGolemAbilityYardChanged OnSnapshotChanged;

private:
	friend class AGolemAbilityYardGameMode;
	void CommitSnapshot();

	UFUNCTION()
	void OnRep_Snapshot();

	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FGolemAbilityYardSnapshot Snapshot;

	UPROPERTY(Replicated)
	TObjectPtr<AGolemAbilityDummy> Dummy;
};
