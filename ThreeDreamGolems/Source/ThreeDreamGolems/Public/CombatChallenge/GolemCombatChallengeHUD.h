#pragma once

#include "AbilityYard/GolemAbilityYardHUD.h"
#include "Components/GolemCombatComponent.h"
#include "GameplayEffectTypes.h"
#include "GolemCombatChallengeHUD.generated.h"

class AGolemCombatChallengeState;
class AGolemPlayerController;
class UGolemAbilitySystemComponent;

/** Challenge presentation caches snapshots and owns every source subscription it creates. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemCombatChallengeHUD : public AGolemAbilityYardHUD
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
	void BindChallengeState(AGolemCombatChallengeState* State);
	void HandleChallengeSnapshot();
	void HandleControlledCombatChanged(AGolemCharacter* Previous, AGolemCharacter* Current);
	void HandleCombatSnapshot(const FGolemCombatSnapshot& Snapshot);
	void HandleResourceChanged(const FOnAttributeChangeData& Change);
	void HandleCombatCommandResult(const FGolemResult& Result);
	void RefreshResourceSnapshot();
	void UnbindCombat();

	UPROPERTY(Transient)
	TWeakObjectPtr<AGolemCombatChallengeState> ChallengeSource;
	UPROPERTY(Transient)
	TWeakObjectPtr<AGolemPlayerController> ChallengeController;
	UPROPERTY(Transient)
	TWeakObjectPtr<UGolemCombatComponent> ControlledCombat;
	UPROPERTY(Transient)
	TWeakObjectPtr<UGolemAbilitySystemComponent> ResourceSource;

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
