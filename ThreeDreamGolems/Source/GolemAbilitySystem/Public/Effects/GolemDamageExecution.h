#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "GolemDamageExecution.generated.h"

UCLASS()
class GOLEMABILITYSYSTEM_API UGolemDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UGolemDamageExecution();
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParameters,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
