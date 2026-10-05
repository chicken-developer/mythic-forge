#include "Effects/KnightArenaResourceRecoveryEffect.h"

#include "Attributes/KnightAttributeSet.h"

UKnightArenaResourceRecoveryEffect::UKnightArenaResourceRecoveryEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(1.0f);
	bExecutePeriodicEffectOnApplication = false;
	FGameplayModifierInfo StaminaRecovery;
	StaminaRecovery.Attribute = UKnightAttributeSet::GetStaminaAttribute();
	StaminaRecovery.ModifierOp = EGameplayModOp::Additive;
	StaminaRecovery.ModifierMagnitude = FScalableFloat(8.0f);
	Modifiers.Add(StaminaRecovery);
	FGameplayModifierInfo RuneRecovery;
	RuneRecovery.Attribute = UKnightAttributeSet::GetRuneEnergyAttribute();
	RuneRecovery.ModifierOp = EGameplayModOp::Additive;
	RuneRecovery.ModifierMagnitude = FScalableFloat(6.0f);
	Modifiers.Add(RuneRecovery);
}
