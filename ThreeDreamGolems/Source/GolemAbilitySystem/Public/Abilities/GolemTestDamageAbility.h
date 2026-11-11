#pragma once

#include "CoreMinimal.h"
#include "Abilities/GolemGameplayAbility.h"
#include "GolemTestDamageAbility.generated.h"

/** A foundation pulse exercises authoritative GAS execution without introducing the combat milestone. */
UCLASS()
class GOLEMABILITYSYSTEM_API UGolemTestDamageAbility : public UGolemGameplayAbility
{
	GENERATED_BODY()

public:
	UGolemTestDamageAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
