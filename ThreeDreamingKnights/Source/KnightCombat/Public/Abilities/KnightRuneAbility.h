#pragma once

#include "CoreMinimal.h"
#include "Abilities/KnightCombatAbility.h"
#include "KnightRuneAbility.generated.h"

UCLASS()
class KNIGHTCOMBAT_API UKnightRuneAbility : public UKnightCombatAbility
{
	GENERATED_BODY()

public:
	UKnightRuneAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
