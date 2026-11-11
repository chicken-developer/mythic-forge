#include "Effects/GolemSourceProjectionEffect.h"

#include "Attributes/GolemAttributeSet.h"

const FName UGolemSourceProjectionEffect::MaxHealthMagnitude(TEXT("Golem.Projection.MaxHealth"));
const FName UGolemSourceProjectionEffect::MaxStaminaMagnitude(TEXT("Golem.Projection.MaxStamina"));
const FName UGolemSourceProjectionEffect::MaxRuneEnergyMagnitude(TEXT("Golem.Projection.MaxRuneEnergy"));
const FName UGolemSourceProjectionEffect::AttackPowerMagnitude(TEXT("Golem.Projection.AttackPower"));
const FName UGolemSourceProjectionEffect::DefenseMagnitude(TEXT("Golem.Projection.Defense"));
const FName UGolemSourceProjectionEffect::CritChanceMagnitude(TEXT("Golem.Projection.CritChance"));
const FName UGolemSourceProjectionEffect::HasteMagnitude(TEXT("Golem.Projection.Haste"));
const FName UGolemSourceProjectionEffect::CooldownRecoveryMagnitude(TEXT("Golem.Projection.CooldownRecovery"));
const FName UGolemSourceProjectionEffect::MasteryMagnitude(TEXT("Golem.Projection.Mastery"));
const FName UGolemSourceProjectionEffect::VersatilityMagnitude(TEXT("Golem.Projection.Versatility"));
const FName UGolemSourceProjectionEffect::PoiseMagnitude(TEXT("Golem.Projection.Poise"));

UGolemSourceProjectionEffect::UGolemSourceProjectionEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	const FGameplayAttribute Attributes[] { UGolemAttributeSet::GetMaxHealthAttribute(),
		UGolemAttributeSet::GetMaxStaminaAttribute(), UGolemAttributeSet::GetMaxRuneEnergyAttribute(),
		UGolemAttributeSet::GetAttackPowerAttribute(), UGolemAttributeSet::GetDefenseAttribute(),
		UGolemAttributeSet::GetCritChanceAttribute(), UGolemAttributeSet::GetHasteAttribute(),
		UGolemAttributeSet::GetCooldownRecoveryAttribute(), UGolemAttributeSet::GetMasteryBonusAttribute(),
		UGolemAttributeSet::GetVersatilityAttribute(), UGolemAttributeSet::GetPoiseBonusAttribute() };
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
