#pragma once

#include "CoreMinimal.h"
#include "Abilities/KnightCombatAbility.h"
#include "KnightFinisherAbility.generated.h"

class UKnightFinisherComponent;

UCLASS()
class KNIGHTCOMBAT_API UKnightFinisherAbility : public UKnightCombatAbility
{
	GENERATED_BODY()

public:
	UKnightFinisherAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void HandleFinisherEnded(FGuid Handle, bool bCompleted);
	TWeakObjectPtr<UKnightFinisherComponent> Finisher;
	FGuid Reservation;
	FDelegateHandle EndedHandle;
};
