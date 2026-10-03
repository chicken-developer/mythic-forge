#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Types/KnightResult.h"
#include "KnightGameplayAbility.generated.h"

class UKnightAbilitySystemComponent;

/** Foundation execution is server-only. Both human input and companion decisions invoke this same GAS action. */
UCLASS(Abstract)
class KNIGHTABILITYSYSTEM_API UKnightGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UKnightGameplayAbility();
	virtual bool ShouldCancelOnInputRelease() const;
	virtual bool DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	UKnightAbilitySystemComponent* GetKnightAbilitySystem() const;
	void ReportResult(const FKnightResult& Result) const;
};
