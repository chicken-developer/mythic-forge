#pragma once

#include "CoreMinimal.h"
#include "Abilities/KnightGameplayAbility.h"
#include "KnightReviveAbility.generated.h"

class UKnightLifeComponent;

/** A held, server-only channel with an exclusive target reservation and validation at both ends. */
UCLASS()
class KNIGHTABILITYSYSTEM_API UKnightReviveAbility : public UKnightGameplayAbility
{
	GENERATED_BODY()

public:
	UKnightReviveAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void CompleteRevive();
	UPROPERTY(Transient)
	TWeakObjectPtr<UKnightLifeComponent> TargetLife;
	FGuid ReservationToken;
	FTimerHandle ReviveTimer;
	bool bChannelCueActive = false;
};
