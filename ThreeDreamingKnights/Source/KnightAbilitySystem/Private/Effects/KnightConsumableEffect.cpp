#include "Effects/KnightConsumableEffect.h"

#include "Attributes/KnightAttributeSet.h"

const FName UKnightConsumableEffect::HealthMagnitude(TEXT("Knight.Item.Health"));
const FName UKnightConsumableEffect::ResourceMagnitude(TEXT("Knight.Item.Resource"));

UKnightConsumableEffect::UKnightConsumableEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	const FGameplayAttribute Attributes[] { UKnightAttributeSet::GetHealthAttribute(), UKnightAttributeSet::GetRuneEnergyAttribute() };
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
