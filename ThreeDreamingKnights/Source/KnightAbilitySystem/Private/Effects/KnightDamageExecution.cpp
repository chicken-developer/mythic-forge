#include "Effects/KnightDamageExecution.h"

#include "Attributes/KnightAttributeSet.h"
#include "Components/ActorComponent.h"
#include "Damage/KnightDamageResponse.h"
#include "GameFramework/Actor.h"
#include "Tags/KnightGameplayTags.h"

namespace
{
	struct FKnightDamageCaptures
	{
		FGameplayEffectAttributeCaptureDefinition DefenseDef;

		FKnightDamageCaptures()
		{
			DefenseDef = FGameplayEffectAttributeCaptureDefinition(UKnightAttributeSet::GetDefenseAttribute(),
				EGameplayEffectAttributeCaptureSource::Target, false);
		}
	};

	const FKnightDamageCaptures& DamageCaptures()
	{
		static const FKnightDamageCaptures Captures;
		return Captures;
	}
}

UKnightDamageExecution::UKnightDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageCaptures().DefenseDef);
}

void UKnightDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Parameters,
	FGameplayEffectCustomExecutionOutput& Output) const
{
	UAbilitySystemComponent* Target = Parameters.GetTargetAbilitySystemComponent();
	if (Target == nullptr || Target->GetOwnerActor() == nullptr || !Target->GetOwnerActor()->HasAuthority()
		|| Target->HasMatchingGameplayTag(KnightGameplayTags::State_Life_Dead))
	{
		return;
	}
	const FGameplayEffectSpec& Spec = Parameters.GetOwningSpec();
	const float RawDamage = Spec.GetSetByCallerMagnitude(KnightGameplayTags::Data_Damage, false, 0.0f);
	if (!FMath::IsFinite(RawDamage) || RawDamage <= 0.0f)
	{
		return;
	}
	FAggregatorEvaluateParameters Evaluation;
	Evaluation.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Evaluation.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	float Defense = 0.0f;
	Parameters.AttemptCalculateCapturedAttributeMagnitude(DamageCaptures().DefenseDef, Evaluation, Defense);
	float Damage = FMath::Max(0.0f, RawDamage - FMath::Max(0.0f, Defense));
	const float RawPoiseDamage = Spec.GetSetByCallerMagnitude(KnightGameplayTags::Data_DamagePoise, false, 0.0f);
	const float PoiseDamage = FMath::IsFinite(RawPoiseDamage) ? FMath::Clamp(RawPoiseDamage, 0.0f, 1000000.0f) : 0.0f;
	const bool bCanDefend = Spec.GetSetByCallerMagnitude(KnightGameplayTags::Data_DamageCanDefend, false, 0.0f) > 0.0f;
	if ((bCanDefend || PoiseDamage > 0.0f) && FMath::IsFinite(Damage))
	{
		AActor* TargetActor = Target->GetOwnerActor();
		IKnightDamageResponse* Response = Cast<IKnightDamageResponse>(TargetActor);
		if (Response == nullptr)
		{
			TInlineComponentArray<UActorComponent*> Components(TargetActor);
			for (UActorComponent* Component : Components)
			{
				if (IsValid(Component))
				{
					Response = Cast<IKnightDamageResponse>(Component);
					if (Response != nullptr)
					{
						break;
					}
				}
			}
		}
		if (Response != nullptr)
		{
			const float UnresolvedDamage = Damage;
			Response->ResolveIncomingDamage(Spec.GetContext().GetOriginalInstigator(), UnresolvedDamage,
				PoiseDamage, bCanDefend, Damage);
			// A response can mitigate damage; it cannot increase or inject a non-finite health mutation.
			Damage = FMath::IsFinite(Damage) ? FMath::Clamp(Damage, 0.0f, UnresolvedDamage) : UnresolvedDamage;
		}
	}
	if (Damage > 0.0f && FMath::IsFinite(Damage))
	{
		Output.AddOutputModifier(FGameplayModifierEvaluatedData(UKnightAttributeSet::GetIncomingDamageAttribute(),
			EGameplayModOp::Additive, Damage));
	}
}
