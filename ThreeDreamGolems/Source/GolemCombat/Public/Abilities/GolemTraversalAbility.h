#pragma once

#include "CoreMinimal.h"
#include "Abilities/GolemCombatAbility.h"
#include "GolemTraversalAbility.generated.h"

class UGolemTraversalComponent;

UCLASS()
class GOLEMCOMBAT_API UGolemTraversalAbility : public UGolemCombatAbility
{
	GENERATED_BODY()

public:
	UGolemTraversalAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void HandleTraversalEnded(FGuid Handle, bool bAborted);
	TWeakObjectPtr<UGolemTraversalComponent> Traversal;
	FGuid TraversalHandle;
	FDelegateHandle EndedHandle;
};
