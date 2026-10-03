#pragma once

#include "CoreMinimal.h"
#include "Abilities/KnightCombatAbility.h"
#include "KnightTraversalAbility.generated.h"

class UKnightTraversalComponent;

UCLASS()
class KNIGHTCOMBAT_API UKnightTraversalAbility : public UKnightCombatAbility
{
	GENERATED_BODY()

public:
	UKnightTraversalAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void HandleTraversalEnded(FGuid Handle, bool bAborted);
	TWeakObjectPtr<UKnightTraversalComponent> Traversal;
	FGuid TraversalHandle;
	FDelegateHandle EndedHandle;
};
