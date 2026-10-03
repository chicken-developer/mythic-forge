#pragma once

#include "CoreMinimal.h"
#include "Components/KnightLifeComponent.h"
#include "Training/KnightTrainingHUD.h"
#include "KnightAbilityYardHUD.generated.h"

class AKnightAbilityYardState;
class AKnightCharacter;
class AKnightPlayerController;
class UKnightAbilitySystemComponent;
struct FOnAttributeChangeData;
struct FKnightResult;

/** Exercise presentation caches snapshots and owned subscriptions; drawing never queries gameplay actors. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightAbilityYardHUD : public AKnightTrainingHUD
{
	GENERATED_BODY()

public:
	float GetDisplayedHealth() const;
	EKnightLifeState GetDisplayedLifeState() const;
	bool IsDisplayingAbilityCompletion() const;
	virtual void DrawHUD() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleActorSpawned(AActor* Actor);
	void BindYardState(AKnightAbilityYardState* State);
	void HandleYardSnapshot();
	void HandleControlledKnightChanged(AKnightCharacter* Previous, AKnightCharacter* Current);
	void HandleHealthChanged(const FOnAttributeChangeData& Change);
	void HandleLifeChanged(EKnightLifeState Previous, EKnightLifeState Current);
	void HandleGameplayCue(FGameplayTag Tag, EGameplayCueEvent::Type EventType);
	void HandleAbilityGrantsChanged();
	void HandleAbilityCommandResult(const FKnightResult& Result);
	void RefreshKnightSnapshot();
	void UnbindKnight();

	UPROPERTY(Transient)
	TWeakObjectPtr<AKnightAbilityYardState> YardSource;

	UPROPERTY(Transient)
	TWeakObjectPtr<AKnightPlayerController> AbilityControlSource;

	UPROPERTY(Transient)
	TWeakObjectPtr<UKnightAbilitySystemComponent> AbilitySource;

	UPROPERTY(Transient)
	TWeakObjectPtr<UKnightLifeComponent> LifeSource;

	FText HealthStatus;
	FText LifeStatus;
	FText DummyStatus;
	FText ObjectiveStatus;
	FText CueStatus;
	FText AbilityCommandStatus;
	float DisplayedHealth = 0.0f;
	EKnightLifeState DisplayedLifeState = EKnightLifeState::Alive;
	bool bCompleted = false;
	FDelegateHandle SpawnHandle;
	FDelegateHandle YardHandle;
	FDelegateHandle ControlHandle;
	FDelegateHandle HealthHandle;
	FDelegateHandle MaxHealthHandle;
	FDelegateHandle LifeHandle;
	FDelegateHandle CueHandle;
	FDelegateHandle GrantsHandle;
	FDelegateHandle AbilityCommandHandle;
};
