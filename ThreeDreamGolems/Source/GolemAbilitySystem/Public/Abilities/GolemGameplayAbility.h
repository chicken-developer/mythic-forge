#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Types/GolemResult.h"
#include "GolemGameplayAbility.generated.h"

class UGolemAbilitySystemComponent;

/** Foundation execution is server-only. Both human input and companion decisions invoke this same GAS action. */
UCLASS(Abstract)
class GOLEMABILITYSYSTEM_API UGolemGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGolemGameplayAbility();
	virtual bool ShouldCancelOnInputRelease() const;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual bool DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	UGolemAbilitySystemComponent* GetGolemAbilitySystem() const;
	void ReportResult(const FGolemResult& Result) const;
};
