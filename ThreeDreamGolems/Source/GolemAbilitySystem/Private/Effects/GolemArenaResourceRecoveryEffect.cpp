#include "Effects/GolemArenaResourceRecoveryEffect.h"

#include "Attributes/GolemAttributeSet.h"

UGolemArenaResourceRecoveryEffect::UGolemArenaResourceRecoveryEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(1.0f);
	bExecutePeriodicEffectOnApplication = false;
	FGameplayModifierInfo StaminaRecovery;
	StaminaRecovery.Attribute = UGolemAttributeSet::GetStaminaAttribute();
	StaminaRecovery.ModifierOp = EGameplayModOp::Additive;
	StaminaRecovery.ModifierMagnitude = FScalableFloat(8.0f);
	Modifiers.Add(StaminaRecovery);
	FGameplayModifierInfo RuneRecovery;
	RuneRecovery.Attribute = UGolemAttributeSet::GetRuneEnergyAttribute();
	RuneRecovery.ModifierOp = EGameplayModOp::Additive;
	RuneRecovery.ModifierMagnitude = FScalableFloat(6.0f);
	Modifiers.Add(RuneRecovery);
}
