#pragma once

#include "CoreMinimal.h"
#include "Abilities/GolemCombatAbility.h"
#include "GolemAttackAbility.generated.h"

UCLASS(Abstract)
class GOLEMCOMBAT_API UGolemAttackAbility : public UGolemCombatAbility
{
	GENERATED_BODY()

public:
	UGolemAttackAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	FGameplayTag ActionTag;

private:
	void CheckAttackMontage();
};
