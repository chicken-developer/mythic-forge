#pragma once

#include "CoreMinimal.h"
#include "Abilities/KnightGameplayAbility.h"
#include "KnightCombatAbility.generated.h"

class UKnightCombatComponent;
class UAnimMontage;

UCLASS(Abstract)
class KNIGHTCOMBAT_API UKnightCombatAbility : public UKnightGameplayAbility
{
	GENERATED_BODY()

public:
	UKnightCombatAbility();
	virtual bool ShouldCancelOnInputRelease() const override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UKnightCombatComponent* GetCombat() const;
	bool StartActionMontage(UAnimMontage* Montage, FName Section = NAME_None);
	void FinishAction();
	void SetActionDuration(float Seconds);
	FTimerHandle ActionTimer;
};
