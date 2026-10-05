#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Events/KnightConfirmedEvent.h"
#include "GameplayEffectTypes.h"
#include "Types/KnightResult.h"
#include "KnightLifeComponent.generated.h"

class UKnightAbilitySystemComponent;

UENUM(BlueprintType)
enum class EKnightLifeState : uint8
{
	Alive,
	Downed,
	Dead
};

USTRUCT()
struct KNIGHTABILITYSYSTEM_API FKnightLifeStateChangedEvent : public FKnightConfirmedEvent
{
	GENERATED_BODY()

	UPROPERTY()
	EKnightLifeState Previous = EKnightLifeState::Alive;
	UPROPERTY()
	EKnightLifeState Current = EKnightLifeState::Alive;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKnightLifeStateChanged, EKnightLifeState, EKnightLifeState);

/** Life state is persistent Knight state. Controller changes cancel channels but do not restart bleedout. */
UCLASS(ClassGroup = Knight, meta = (BlueprintSpawnableComponent))
class KNIGHTABILITYSYSTEM_API UKnightLifeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKnightLifeComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FKnightResult InitializeLife(UKnightAbilitySystemComponent* InAbilitySystem);
	EKnightLifeState GetLifeState() const;
	bool IsAlive() const;
	bool IsDowned() const;
	bool IsDead() const;
	float GetReviveDuration() const;
	float GetReviveRange() const;
	double GetBleedoutEndServerTime() const;
	bool IsReviveInProgress() const;
	AActor* GetActiveReviver() const;
	FKnightResult ValidateReviveSource(AActor* Reviver) const;
	FKnightResult TryBeginRevive(AActor* Reviver, FGuid& OutToken);
	FKnightResult TryCompleteRevive(AActor* Reviver, const FGuid& Token);
	FKnightResult CancelRevive(AActor* Reviver, const FGuid& Token);
	FKnightResult ResetForCheckpoint();
	/** Composition restores every party member and AI context before explicitly restarting decisions. */
	FKnightResult ResetForCheckpointWithDeferredControllerRestart();
	bool ShouldDeferControllerRestart() const;
	FKnightResult TrySetLifeTuning(float InBleedoutDuration, float InReviveDuration,
		float InReviveRange, float InReviveHealthFraction);
	void NotifyDamageWhileDowned();
	FOnKnightLifeStateChanged OnLifeStateChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& Change);
	void HandleBleedout();
	void ChangeLifeState(EKnightLifeState NewState);
	void ReleaseReviveReservation(bool bCancelSourceAbility);
	void SynchronizeLifeTags();

	UFUNCTION()
	void OnRep_LifeState(EKnightLifeState Previous);

	UPROPERTY(ReplicatedUsing = OnRep_LifeState)
	EKnightLifeState LifeState = EKnightLifeState::Alive;
	UPROPERTY(Replicated)
	double BleedoutEndServerTime = 0.0;
	UPROPERTY(Transient)
	TWeakObjectPtr<UKnightAbilitySystemComponent> AbilitySystem;
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> ActiveReviver;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|GAS|Life", meta = (ClampMin = "0.1", Units = "s"))
	float BleedoutDuration = 20.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|GAS|Life", meta = (ClampMin = "0.1", Units = "s"))
	float ReviveDuration = 2.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|GAS|Life", meta = (ClampMin = "1", Units = "cm"))
	float ReviveRange = 300.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|GAS|Life", meta = (ClampMin = "0.01", ClampMax = "1"))
	float ReviveHealthFraction = 0.35f;

	FGuid ReviveToken;
	double ReviveStartedServerTime = 0.0;
	FTimerHandle BleedoutTimer;
	FDelegateHandle HealthChangedHandle;
	int32 LifeRevision = 0;
	bool bDeferControllerRestart = false;
};
