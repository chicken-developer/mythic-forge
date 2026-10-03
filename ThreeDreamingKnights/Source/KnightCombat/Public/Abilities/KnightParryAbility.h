#pragma once

#include "CoreMinimal.h"
#include "Abilities/KnightCombatAbility.h"
#include "KnightParryAbility.generated.h"

UCLASS()
class KNIGHTCOMBAT_API UKnightParryAbility : public UKnightCombatAbility
{
	GENERATED_BODY()

public:
	UKnightParryAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
};
