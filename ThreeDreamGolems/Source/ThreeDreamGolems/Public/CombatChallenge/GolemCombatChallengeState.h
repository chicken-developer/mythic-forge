#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GolemCombatChallengeState.generated.h"

USTRUCT(BlueprintType)
struct THREEDREAMGOLEMS_API FGolemCombatChallengeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|CombatChallenge")
	int32 Revision = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|CombatChallenge")
	int32 LightHitCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|CombatChallenge")
	int32 HeavyHitCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|CombatChallenge")
	int32 ComboCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|CombatChallenge")
	int32 BlockCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|CombatChallenge")
	int32 ParryCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|CombatChallenge")
	int32 DodgeCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|CombatChallenge")
	int32 StaggerCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|CombatChallenge")
	int32 RuneHitCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|CombatChallenge")
	int32 FinisherCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|CombatChallenge")
	int32 TraversalCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|CombatChallenge")
	bool bTargetLocked = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|CombatChallenge")
	bool bCompleted = false;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|CombatChallenge")
	bool bFailed = false;
};

DECLARE_MULTICAST_DELEGATE(FOnGolemCombatChallengeChanged);

/** Challenge progress is committed only from authoritative domain confirmations. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemCombatChallengeState : public AActor
{
	GENERATED_BODY()

public:
	AGolemCombatChallengeState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	static AGolemCombatChallengeState* Find(UWorld* World);
	const FGolemCombatChallengeSnapshot& GetSnapshot() const;
	FOnGolemCombatChallengeChanged OnSnapshotChanged;

private:
	friend class AGolemCombatChallengeGameMode;
	void CommitSnapshot();
	UFUNCTION()
	void OnRep_Snapshot();
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FGolemCombatChallengeSnapshot Snapshot;
};
