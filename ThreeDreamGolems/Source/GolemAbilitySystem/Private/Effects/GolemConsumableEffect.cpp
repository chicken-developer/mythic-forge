#include "Effects/GolemConsumableEffect.h"

#include "Attributes/GolemAttributeSet.h"

const FName UGolemConsumableEffect::HealthMagnitude(TEXT("Golem.Item.Health"));
const FName UGolemConsumableEffect::ResourceMagnitude(TEXT("Golem.Item.Resource"));

UGolemConsumableEffect::UGolemConsumableEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	const FGameplayAttribute Attributes[] { UGolemAttributeSet::GetHealthAttribute(), UGolemAttributeSet::GetRuneEnergyAttribute() };
	const FName Names[] { HealthMagnitude, ResourceMagnitude };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Attributes); ++Index)
	{
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attributes[Index];
		Modifier.ModifierOp = EGameplayModOp::Additive;
		FSetByCallerFloat Magnitude;
		Magnitude.DataName = Names[Index];
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
		Modifiers.Add(Modifier);
	}
}
