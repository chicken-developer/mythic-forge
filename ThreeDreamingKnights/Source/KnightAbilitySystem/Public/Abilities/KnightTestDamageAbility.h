#pragma once

#include "CoreMinimal.h"
#include "Abilities/KnightGameplayAbility.h"
#include "KnightTestDamageAbility.generated.h"

/** A foundation pulse exercises authoritative GAS execution without introducing the combat milestone. */
UCLASS()
class KNIGHTABILITYSYSTEM_API UKnightTestDamageAbility : public UKnightGameplayAbility
{
	GENERATED_BODY()

public:
	UKnightTestDamageAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
