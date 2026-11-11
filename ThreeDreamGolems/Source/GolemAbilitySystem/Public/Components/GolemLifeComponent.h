#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Events/GolemConfirmedEvent.h"
#include "GameplayEffectTypes.h"
#include "Types/GolemResult.h"
#include "GolemLifeComponent.generated.h"

class UGolemAbilitySystemComponent;

UENUM(BlueprintType)
enum class EGolemLifeState : uint8
{
	Alive,
	Downed,
	Dead
};

USTRUCT()
struct GOLEMABILITYSYSTEM_API FGolemLifeStateChangedEvent : public FGolemConfirmedEvent
{
	GENERATED_BODY()

	UPROPERTY()
	EGolemLifeState Previous = EGolemLifeState::Alive;
	UPROPERTY()
	EGolemLifeState Current = EGolemLifeState::Alive;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGolemLifeStateChanged, EGolemLifeState, EGolemLifeState);

/** Life state is persistent Golem state. Controller changes cancel channels but do not restart bleedout. */
UCLASS(ClassGroup = Golem, meta = (BlueprintSpawnableComponent))
class GOLEMABILITYSYSTEM_API UGolemLifeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGolemLifeComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGolemResult InitializeLife(UGolemAbilitySystemComponent* InAbilitySystem);
	EGolemLifeState GetLifeState() const;
	bool IsAlive() const;
	bool IsDowned() const;
	bool IsDead() const;
	float GetReviveDuration() const;
	float GetReviveRange() const;
	double GetBleedoutEndServerTime() const;
	bool IsReviveInProgress() const;
	AActor* GetActiveReviver() const;
	FGolemResult ValidateReviveSource(AActor* Reviver) const;
	FGolemResult TryBeginRevive(AActor* Reviver, FGuid& OutToken);
	FGolemResult TryCompleteRevive(AActor* Reviver, const FGuid& Token);
	FGolemResult CancelRevive(AActor* Reviver, const FGuid& Token);
	FGolemResult ResetForCheckpoint();
	/** Composition restores every party member and AI context before explicitly restarting decisions. */
	FGolemResult ResetForCheckpointWithDeferredControllerRestart();
	bool ShouldDeferControllerRestart() const;
	FGolemResult TrySetLifeTuning(float InBleedoutDuration, float InReviveDuration,
		float InReviveRange, float InReviveHealthFraction);
	void NotifyDamageWhileDowned();
	FOnGolemLifeStateChanged OnLifeStateChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& Change);
	void HandleBleedout();
	void ChangeLifeState(EGolemLifeState NewState);
	void ReleaseReviveReservation(bool bCancelSourceAbility);
	void SynchronizeLifeTags();

	UFUNCTION()
	void OnRep_LifeState(EGolemLifeState Previous);

	UPROPERTY(ReplicatedUsing = OnRep_LifeState)
	EGolemLifeState LifeState = EGolemLifeState::Alive;
	UPROPERTY(Replicated)
	double BleedoutEndServerTime = 0.0;
	UPROPERTY(Transient)
	TWeakObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> ActiveReviver;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|GAS|Life", meta = (ClampMin = "0.1", Units = "s"))
	float BleedoutDuration = 20.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|GAS|Life", meta = (ClampMin = "0.1", Units = "s"))
	float ReviveDuration = 2.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|GAS|Life", meta = (ClampMin = "1", Units = "cm"))
	float ReviveRange = 300.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|GAS|Life", meta = (ClampMin = "0.01", ClampMax = "1"))
	float ReviveHealthFraction = 0.35f;

	FGuid ReviveToken;
	double ReviveStartedServerTime = 0.0;
	FTimerHandle BleedoutTimer;
	FDelegateHandle HealthChangedHandle;
	int32 LifeRevision = 0;
	bool bDeferControllerRestart = false;
};
