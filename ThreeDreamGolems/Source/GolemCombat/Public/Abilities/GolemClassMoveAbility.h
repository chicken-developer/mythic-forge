#pragma once

#include "CoreMinimal.h"
#include "Abilities/GolemCombatAbility.h"
#include "GolemClassMoveAbility.generated.h"

/**
 * The class move button (DEV_SPEC section 4). One ability for all three classes: the Golem's hero
 * data says whether it is a Warrior's Guard (held), a Hunter's backflip shot or a Mage's barrier.
 * An actor without hero data gets a plain Guard with no time limit and no recovery.
 */
UCLASS()
class GOLEMCOMBAT_API UGolemClassMoveAbility : public UGolemCombatAbility
{
	GENERATED_BODY()

public:
	UGolemClassMoveAbility();

	/** Only a Guard ends when the button is let go; the other two moves run to their end. */
	virtual bool ShouldCancelOnInputRelease() const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	bool bHeldMove = true;
};
