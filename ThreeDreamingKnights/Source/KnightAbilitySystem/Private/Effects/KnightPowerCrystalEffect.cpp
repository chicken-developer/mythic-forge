#include "Effects/KnightPowerCrystalEffect.h"

#include "Attributes/KnightAttributeSet.h"

const FName UKnightPowerCrystalEffect::PowerMultiplierMagnitude(TEXT("Knight.Crystal.PowerMultiplier"));

UKnightPowerCrystalEffect::UKnightPowerCrystalEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UKnightAttributeSet::GetAttackPowerAttribute();
	Modifier.ModifierOp = EGameplayModOp::Multiplicitive;
	FSetByCallerFloat Magnitude;
	Magnitude.DataName = PowerMultiplierMagnitude;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	Modifiers.Add(Modifier);
}
