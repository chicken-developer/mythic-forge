#include "Effects/KnightDamageEffect.h"

#include "Effects/KnightDamageExecution.h"

UKnightDamageEffect::UKnightDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UKnightDamageExecution::StaticClass();
	Executions.Add(Execution);
}
