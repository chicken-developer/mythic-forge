#include "Effects/KnightSourceProjectionEffect.h"

#include "Attributes/KnightAttributeSet.h"

const FName UKnightSourceProjectionEffect::MaxHealthMagnitude(TEXT("Knight.Projection.MaxHealth"));
const FName UKnightSourceProjectionEffect::MaxStaminaMagnitude(TEXT("Knight.Projection.MaxStamina"));
const FName UKnightSourceProjectionEffect::MaxRuneEnergyMagnitude(TEXT("Knight.Projection.MaxRuneEnergy"));
const FName UKnightSourceProjectionEffect::AttackPowerMagnitude(TEXT("Knight.Projection.AttackPower"));
const FName UKnightSourceProjectionEffect::DefenseMagnitude(TEXT("Knight.Projection.Defense"));
const FName UKnightSourceProjectionEffect::CritChanceMagnitude(TEXT("Knight.Projection.CritChance"));
const FName UKnightSourceProjectionEffect::HasteMagnitude(TEXT("Knight.Projection.Haste"));
const FName UKnightSourceProjectionEffect::CooldownRecoveryMagnitude(TEXT("Knight.Projection.CooldownRecovery"));
const FName UKnightSourceProjectionEffect::MasteryMagnitude(TEXT("Knight.Projection.Mastery"));
const FName UKnightSourceProjectionEffect::VersatilityMagnitude(TEXT("Knight.Projection.Versatility"));
const FName UKnightSourceProjectionEffect::PoiseMagnitude(TEXT("Knight.Projection.Poise"));

UKnightSourceProjectionEffect::UKnightSourceProjectionEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	const FGameplayAttribute Attributes[] { UKnightAttributeSet::GetMaxHealthAttribute(),
		UKnightAttributeSet::GetMaxStaminaAttribute(), UKnightAttributeSet::GetMaxRuneEnergyAttribute(),
		UKnightAttributeSet::GetAttackPowerAttribute(), UKnightAttributeSet::GetDefenseAttribute(),
		UKnightAttributeSet::GetCritChanceAttribute(), UKnightAttributeSet::GetHasteAttribute(),
		UKnightAttributeSet::GetCooldownRecoveryAttribute(), UKnightAttributeSet::GetMasteryBonusAttribute(),
		UKnightAttributeSet::GetVersatilityAttribute(), UKnightAttributeSet::GetPoiseBonusAttribute() };
	const FName Names[] { MaxHealthMagnitude, MaxStaminaMagnitude, MaxRuneEnergyMagnitude,
		AttackPowerMagnitude, DefenseMagnitude, CritChanceMagnitude, HasteMagnitude,
		CooldownRecoveryMagnitude, MasteryMagnitude, VersatilityMagnitude, PoiseMagnitude };
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
