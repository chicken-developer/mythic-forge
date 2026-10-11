#pragma once

#include "CoreMinimal.h"
#include "Abilities/KnightGameplayAbility.h"
#include "KnightSpecializationAbility.generated.h"

class UKnightSkillRuntimeComponent;

/** Four source-owned specs share this execution class; the stable slot selects the catalog behavior. */
UCLASS()
class KNIGHTABILITYSYSTEM_API UKnightSpecializationAbility : public UKnightGameplayAbility
{
	GENERATED_BODY()

public:
	UKnightSpecializationAbility();
	virtual bool ShouldCancelOnInputRelease() const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void CommitAction();
	void FinishRecovery();
	TWeakObjectPtr<UKnightSkillRuntimeComponent> Runtime;
	TWeakObjectPtr<AActor> Target;
	int32 Slot = 0;
	FGuid ActivationId;
	FTimerHandle CommitTimer;
	FTimerHandle RecoveryTimer;
};
