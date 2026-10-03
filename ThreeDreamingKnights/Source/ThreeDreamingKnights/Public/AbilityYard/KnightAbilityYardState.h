#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/KnightStableIds.h"
#include "KnightAbilityYardState.generated.h"

class AKnightAbilityDummy;
class AKnightAbilityYardGameMode;

USTRUCT(BlueprintType)
struct THREEDREAMINGKNIGHTS_API FKnightAbilityYardSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|AbilityYard")
	int32 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|AbilityYard")
	float DummyHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|AbilityYard")
	float DummyMaxHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|AbilityYard")
	bool bDummyDamaged = false;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|AbilityYard")
	bool bPlayerDamaged = false;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|AbilityYard")
	bool bCompanionDowned = false;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|AbilityYard")
	bool bCompanionRevived = false;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|AbilityYard")
	bool bCompleted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|AbilityYard")
	int32 ReviveSlot = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|AbilityYard")
	FKnightEntityId ReviveKnightId;
};

DECLARE_MULTICAST_DELEGATE(FOnKnightAbilityYardChanged);

/** Replicated exercise feedback observes real GAS results; presentation has no mutation API. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightAbilityYardState : public AActor
{
	GENERATED_BODY()

public:
	AKnightAbilityYardState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static AKnightAbilityYardState* Find(UWorld* World);
	const FKnightAbilityYardSnapshot& GetSnapshot() const;
	AKnightAbilityDummy* GetDummy() const;
	FOnKnightAbilityYardChanged OnSnapshotChanged;

private:
	friend class AKnightAbilityYardGameMode;
	void CommitSnapshot();

	UFUNCTION()
	void OnRep_Snapshot();

	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FKnightAbilityYardSnapshot Snapshot;

	UPROPERTY(Replicated)
	TObjectPtr<AKnightAbilityDummy> Dummy;
};
