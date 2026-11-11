#pragma once

#include "CoreMinimal.h"
#include "Abilities/GolemCombatAbility.h"
#include "GolemLockTargetAbility.generated.h"

UCLASS()
class GOLEMCOMBAT_API UGolemLockTargetAbility : public UGolemCombatAbility
{
	GENERATED_BODY()

public:
	UGolemLockTargetAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
