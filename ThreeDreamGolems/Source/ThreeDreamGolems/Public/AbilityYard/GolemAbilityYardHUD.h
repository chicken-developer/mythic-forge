#pragma once

#include "CoreMinimal.h"
#include "Components/GolemLifeComponent.h"
#include "Training/GolemTrainingHUD.h"
#include "GolemAbilityYardHUD.generated.h"

class AGolemAbilityYardState;
class AGolemCharacter;
class AGolemPlayerController;
class UGolemAbilitySystemComponent;
struct FOnAttributeChangeData;
struct FGolemResult;

/** Exercise presentation caches snapshots and owned subscriptions; drawing never queries gameplay actors. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemAbilityYardHUD : public AGolemTrainingHUD
{
	GENERATED_BODY()

public:
	float GetDisplayedHealth() const;
	EGolemLifeState GetDisplayedLifeState() const;
	bool IsDisplayingAbilityCompletion() const;
	virtual void DrawHUD() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleActorSpawned(AActor* Actor);
	void BindYardState(AGolemAbilityYardState* State);
	void HandleYardSnapshot();
	void HandleControlledGolemChanged(AGolemCharacter* Previous, AGolemCharacter* Current);
	void HandleHealthChanged(const FOnAttributeChangeData& Change);
	void HandleLifeChanged(EGolemLifeState Previous, EGolemLifeState Current);
	void HandleGameplayCue(FGameplayTag Tag, EGameplayCueEvent::Type EventType);
	void HandleAbilityGrantsChanged();
	void HandleAbilityCommandResult(const FGolemResult& Result);
	void RefreshGolemSnapshot();
	void UnbindGolem();

	UPROPERTY(Transient)
	TWeakObjectPtr<AGolemAbilityYardState> YardSource;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGolemPlayerController> AbilityControlSource;

	UPROPERTY(Transient)
	TWeakObjectPtr<UGolemAbilitySystemComponent> AbilitySource;

	UPROPERTY(Transient)
	TWeakObjectPtr<UGolemLifeComponent> LifeSource;

	FText HealthStatus;
	FText LifeStatus;
	FText DummyStatus;
	FText ObjectiveStatus;
	FText CueStatus;
	FText AbilityCommandStatus;
	float DisplayedHealth = 0.0f;
	EGolemLifeState DisplayedLifeState = EGolemLifeState::Alive;
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
