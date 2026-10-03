#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "KnightDamageExecution.generated.h"

UCLASS()
class KNIGHTABILITYSYSTEM_API UKnightDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UKnightDamageExecution();
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParameters,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
