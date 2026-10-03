#pragma once

#include "CoreMinimal.h"
#include "Abilities/KnightCombatAbility.h"
#include "KnightLockTargetAbility.generated.h"

UCLASS()
class KNIGHTCOMBAT_API UKnightLockTargetAbility : public UKnightCombatAbility
{
	GENERATED_BODY()

public:
	UKnightLockTargetAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
