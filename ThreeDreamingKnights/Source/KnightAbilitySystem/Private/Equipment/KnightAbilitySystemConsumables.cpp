#include "Components/KnightAbilitySystemComponent.h"

#include "AbilitySystemGlobals.h"
#include "Effects/KnightConsumableEffect.h"

namespace
{
	FKnightResult KnightConsumableEffectFailure(FName Context)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, Context,
			NSLOCTEXT("KnightEquipment", "ConsumableEffectRejected", "The recovery effect cannot be applied."));
	}
}

FKnightResult UKnightAbilitySystemComponent::PrepareConsumableEffect(
	const FKnightMutationFence& Fence, FGameplayEffectSpec& Spec)
{
	const FKnightResult FenceResult = ValidateMutationFence(Fence);
	if (!FenceResult.IsSuccess())
	{
		return FenceResult;
	}
	if (Spec.Def != GetDefault<UKnightConsumableEffect>() || Spec.GetPeriod() != UGameplayEffect::NO_PERIOD
		|| Spec.GetDuration() != UGameplayEffect::INSTANT_APPLICATION
		|| Spec.GetContext().GetInstigatorAbilitySystemComponent() != this)
	{
		return KnightConsumableEffectFailure(TEXT("ConsumableNativeSpec"));
	}
	FScopedActiveGameplayEffectLock EffectLock(ActiveGameplayEffects);
	FScopeCurrentGameplayEffectBeingApplied AppliedScope(&Spec, this);

	// Approval belongs to this exact fenced native instant spec. A later stateful query cannot veto
	// recovery after the inventory receipt has committed; execution below does not repeat approval.
	const TArray<FGameplayEffectApplicationQuery> ApplicationQueries = GameplayEffectApplicationQueries;
	for (const FGameplayEffectApplicationQuery& Query : ApplicationQueries)
	{
		if (!Query.Execute(ActiveGameplayEffects, Spec))
		{
			return KnightConsumableEffectFailure(TEXT("ConsumableApplicationQuery"));
		}
	}
	if (!Spec.Def->CanApply(ActiveGameplayEffects, Spec))
	{
		return KnightConsumableEffectFailure(TEXT("ConsumableApplicationRequirements"));
	}
	for (const FGameplayModifierInfo& Modifier : Spec.Def->Modifiers)
	{
		if (!Modifier.Attribute.IsValid() || !HasAttributeSetForAttribute(Modifier.Attribute))
		{
			return KnightConsumableEffectFailure(TEXT("ConsumableAttributeSet"));
		}
	}
	UAbilitySystemGlobals::Get().GlobalPreGameplayEffectSpecApply(Spec, this);
	if (Spec.Def != GetDefault<UKnightConsumableEffect>())
	{
		return KnightConsumableEffectFailure(TEXT("ConsumableNativeSpecChanged"));
	}
	Spec.CaptureAttributeDataFromTarget(this);
	return ValidateMutationFence(Fence);
}

void UKnightAbilitySystemComponent::ExecutePreparedConsumableEffect(
	const FKnightMutationFence& Fence, FGameplayEffectSpec& Spec)
{
	check(Spec.Def == GetDefault<UKnightConsumableEffect>());
	check(ValidateMutationFence(Fence).IsSuccess());
	FPredictionKey Prediction;
	FScopedActiveGameplayEffectLock EffectLock(ActiveGameplayEffects);
	FScopeCurrentGameplayEffectBeingApplied AppliedScope(&Spec, this);
	ExecuteGameplayEffect(Spec, Prediction);
	Spec.Def->OnApplied(ActiveGameplayEffects, Spec, Prediction);
	const FActiveGameplayEffectHandle Executed = FActiveGameplayEffectHandle::GetInstantExecutedHandle();
	UAbilitySystemComponent* InstigatorASC = Spec.GetContext().GetInstigatorAbilitySystemComponent();
	OnGameplayEffectAppliedToSelf(InstigatorASC, Spec, Executed);
	if (InstigatorASC != nullptr)
	{
		InstigatorASC->OnGameplayEffectAppliedToTarget(this, Spec, Executed);
	}
}
