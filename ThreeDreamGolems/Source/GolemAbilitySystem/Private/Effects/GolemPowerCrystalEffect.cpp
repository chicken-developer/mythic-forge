#include "Effects/GolemPowerCrystalEffect.h"

#include "Attributes/GolemAttributeSet.h"

const FName UGolemPowerCrystalEffect::PowerMultiplierMagnitude(TEXT("Golem.Crystal.PowerMultiplier"));

UGolemPowerCrystalEffect::UGolemPowerCrystalEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UGolemAttributeSet::GetAttackPowerAttribute();
	Modifier.ModifierOp = EGameplayModOp::Multiplicitive;
	FSetByCallerFloat Magnitude;
	Magnitude.DataName = PowerMultiplierMagnitude;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	Modifiers.Add(Modifier);
}
