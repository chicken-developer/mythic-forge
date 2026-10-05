#include "Effects/KnightSourceProjectionEffect.h"

#include "Attributes/KnightAttributeSet.h"

const FName UKnightSourceProjectionEffect::MaxHealthMagnitude(TEXT("Knight.Projection.MaxHealth"));
const FName UKnightSourceProjectionEffect::MaxStaminaMagnitude(TEXT("Knight.Projection.MaxStamina"));
const FName UKnightSourceProjectionEffect::MaxRuneEnergyMagnitude(TEXT("Knight.Projection.MaxRuneEnergy"));
const FName UKnightSourceProjectionEffect::AttackPowerMagnitude(TEXT("Knight.Projection.AttackPower"));
const FName UKnightSourceProjectionEffect::DefenseMagnitude(TEXT("Knight.Projection.Defense"));

UKnightSourceProjectionEffect::UKnightSourceProjectionEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	const FGameplayAttribute Attributes[] { UKnightAttributeSet::GetMaxHealthAttribute(),
		UKnightAttributeSet::GetMaxStaminaAttribute(), UKnightAttributeSet::GetMaxRuneEnergyAttribute(),
		UKnightAttributeSet::GetAttackPowerAttribute(), UKnightAttributeSet::GetDefenseAttribute() };
	const FName Names[] { MaxHealthMagnitude, MaxStaminaMagnitude, MaxRuneEnergyMagnitude,
		AttackPowerMagnitude, DefenseMagnitude };
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
