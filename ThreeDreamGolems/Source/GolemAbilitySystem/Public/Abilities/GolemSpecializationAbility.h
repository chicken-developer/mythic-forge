#pragma once

#include "CoreMinimal.h"
#include "Abilities/GolemGameplayAbility.h"
#include "GolemSpecializationAbility.generated.h"

class UGolemSkillRuntimeComponent;

/** Four source-owned specs share this execution class; the stable slot selects the catalog behavior. */
UCLASS()
class GOLEMABILITYSYSTEM_API UGolemSpecializationAbility : public UGolemGameplayAbility
{
	GENERATED_BODY()

public:
	UGolemSpecializationAbility();
	virtual bool ShouldCancelOnInputRelease() const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void CommitAction();
	void FinishRecovery();
	TWeakObjectPtr<UGolemSkillRuntimeComponent> Runtime;
	TWeakObjectPtr<AActor> Target;
	int32 Slot = 0;
	FGuid ActivationId;
	FTimerHandle CommitTimer;
	FTimerHandle RecoveryTimer;
};
