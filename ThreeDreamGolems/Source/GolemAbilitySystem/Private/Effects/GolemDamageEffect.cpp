#include "Effects/GolemDamageEffect.h"

#include "Effects/GolemDamageExecution.h"

UGolemDamageEffect::UGolemDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UGolemDamageExecution::StaticClass();
	Executions.Add(Execution);
}
