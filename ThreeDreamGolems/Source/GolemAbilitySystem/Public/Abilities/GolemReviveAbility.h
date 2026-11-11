#pragma once

#include "CoreMinimal.h"
#include "Abilities/GolemGameplayAbility.h"
#include "GolemReviveAbility.generated.h"

class UGolemLifeComponent;

/** A held, server-only channel with an exclusive target reservation and validation at both ends. */
UCLASS()
class GOLEMABILITYSYSTEM_API UGolemReviveAbility : public UGolemGameplayAbility
{
	GENERATED_BODY()

public:
	UGolemReviveAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void CompleteRevive();
	UPROPERTY(Transient)
	TWeakObjectPtr<UGolemLifeComponent> TargetLife;
	FGuid ReservationToken;
	FTimerHandle ReviveTimer;
	bool bChannelCueActive = false;
};
