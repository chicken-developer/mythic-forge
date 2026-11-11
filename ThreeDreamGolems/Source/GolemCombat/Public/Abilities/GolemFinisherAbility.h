#pragma once

#include "CoreMinimal.h"
#include "Abilities/GolemCombatAbility.h"
#include "GolemFinisherAbility.generated.h"

class UGolemFinisherComponent;

UCLASS()
class GOLEMCOMBAT_API UGolemFinisherAbility : public UGolemCombatAbility
{
	GENERATED_BODY()

public:
	UGolemFinisherAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void HandleFinisherEnded(FGuid Handle, bool bCompleted);
	TWeakObjectPtr<UGolemFinisherComponent> Finisher;
	FGuid Reservation;
	FDelegateHandle EndedHandle;
};
