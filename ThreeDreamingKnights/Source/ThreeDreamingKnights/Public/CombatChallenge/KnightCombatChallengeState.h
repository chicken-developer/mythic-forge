#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KnightCombatChallengeState.generated.h"

USTRUCT(BlueprintType)
struct THREEDREAMINGKNIGHTS_API FKnightCombatChallengeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|CombatChallenge")
	int32 Revision = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|CombatChallenge")
	int32 LightHitCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|CombatChallenge")
	int32 HeavyHitCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|CombatChallenge")
	int32 ComboCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|CombatChallenge")
	int32 BlockCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|CombatChallenge")
	int32 ParryCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|CombatChallenge")
	int32 DodgeCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|CombatChallenge")
	int32 StaggerCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|CombatChallenge")
	int32 RuneHitCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|CombatChallenge")
	int32 FinisherCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|CombatChallenge")
	int32 TraversalCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|CombatChallenge")
	bool bTargetLocked = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|CombatChallenge")
	bool bCompleted = false;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|CombatChallenge")
	bool bFailed = false;
};

DECLARE_MULTICAST_DELEGATE(FOnKnightCombatChallengeChanged);

/** Challenge progress is committed only from authoritative domain confirmations. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightCombatChallengeState : public AActor
{
	GENERATED_BODY()

public:
	AKnightCombatChallengeState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static AKnightCombatChallengeState* Find(UWorld* World);
	const FKnightCombatChallengeSnapshot& GetSnapshot() const;
	FOnKnightCombatChallengeChanged OnSnapshotChanged;

private:
	friend class AKnightCombatChallengeGameMode;
	void CommitSnapshot();
	UFUNCTION()
	void OnRep_Snapshot();
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FKnightCombatChallengeSnapshot Snapshot;
};
