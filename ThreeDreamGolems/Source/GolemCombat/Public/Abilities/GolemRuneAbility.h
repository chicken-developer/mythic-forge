#pragma once

#include "CoreMinimal.h"
#include "Abilities/GolemCombatAbility.h"
#include "GolemRuneAbility.generated.h"

UCLASS()
class GOLEMCOMBAT_API UGolemRuneAbility : public UGolemCombatAbility
{
	GENERATED_BODY()

public:
	UGolemRuneAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
