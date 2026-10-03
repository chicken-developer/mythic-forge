#pragma once

#include "AbilityYard/KnightAbilityYardHUD.h"
#include "Components/KnightCombatComponent.h"
#include "GameplayEffectTypes.h"
#include "KnightCombatChallengeHUD.generated.h"

class AKnightCombatChallengeState;
class AKnightPlayerController;
class UKnightAbilitySystemComponent;

/** Challenge presentation caches snapshots and owns every source subscription it creates. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightCombatChallengeHUD : public AKnightAbilityYardHUD
{
	GENERATED_BODY()

public:
	bool IsDisplayingChallengeCompletion() const;
	bool IsDisplayingChallengeFailure() const;
	virtual void DrawHUD() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleChallengeActorSpawned(AActor* Actor);
	void BindChallengeState(AKnightCombatChallengeState* State);
	void HandleChallengeSnapshot();
	void HandleControlledCombatChanged(AKnightCharacter* Previous, AKnightCharacter* Current);
	void HandleCombatSnapshot(const FKnightCombatSnapshot& Snapshot);
	void HandleResourceChanged(const FOnAttributeChangeData& Change);
	void HandleCombatCommandResult(const FKnightResult& Result);
	void RefreshResourceSnapshot();
	void UnbindCombat();

	UPROPERTY(Transient)
	TWeakObjectPtr<AKnightCombatChallengeState> ChallengeSource;
	UPROPERTY(Transient)
	TWeakObjectPtr<AKnightPlayerController> ChallengeController;
	UPROPERTY(Transient)
	TWeakObjectPtr<UKnightCombatComponent> ControlledCombat;
	UPROPERTY(Transient)
	TWeakObjectPtr<UKnightAbilitySystemComponent> ResourceSource;

	FDelegateHandle ChallengeSpawnHandle;
	FDelegateHandle ChallengeStateHandle;
	FDelegateHandle ChallengeControlHandle;
	FDelegateHandle ChallengeCommandHandle;
	FDelegateHandle CombatSnapshotHandle;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> ResourceHandles;
	FText ResourceStatus;
	FText CombatStatus;
	FText AttackProgress;
	FText DefenseProgress;
	FText ActionProgress;
	FText ChallengeCommandStatus;
	bool bChallengeCompleted = false;
	bool bChallengeFailed = false;
};
