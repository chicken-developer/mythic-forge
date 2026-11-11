#pragma once

#include "CoreMinimal.h"
#include "Abilities/GolemGameplayAbility.h"
#include "GolemCombatAbility.generated.h"

class UGolemCombatComponent;
class UAnimMontage;

UCLASS(Abstract)
class GOLEMCOMBAT_API UGolemCombatAbility : public UGolemGameplayAbility
{
	GENERATED_BODY()

public:
	UGolemCombatAbility();
	virtual bool ShouldCancelOnInputRelease() const override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UGolemCombatComponent* GetCombat() const;
	bool StartActionMontage(UAnimMontage* Montage, FName Section = NAME_None);
	void FinishAction();
	void SetActionDuration(float Seconds);
	FTimerHandle ActionTimer;
};
