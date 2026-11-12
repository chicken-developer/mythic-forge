#include "Effects/GolemDamageExecution.h"

#include "Attributes/GolemAttributeSet.h"
#include "Components/ActorComponent.h"
#include "Damage/GolemDamageResponse.h"
#include "GameFramework/Actor.h"
#include "Tags/GolemGameplayTags.h"

namespace
{
	constexpr float GolemMaxDamagePacket = 1000000.0f;

	IGolemDamageResponse* FindDamageResponse(AActor* Target)
	{
		if (IGolemDamageResponse* Response = Cast<IGolemDamageResponse>(Target))
		{
			return Response;
		}
		TInlineComponentArray<UActorComponent*> Components(Target);
		for (UActorComponent* Component : Components)
		{
			if (IGolemDamageResponse* Response = IsValid(Component) ? Cast<IGolemDamageResponse>(Component) : nullptr)
			{
				return Response;
			}
		}
		return nullptr;
	}
}

UGolemDamageExecution::UGolemDamageExecution()
{
}

void UGolemDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Parameters,
	FGameplayEffectCustomExecutionOutput& Output) const
{
	UAbilitySystemComponent* Target = Parameters.GetTargetAbilitySystemComponent();
	if (Target == nullptr || Target->GetOwnerActor() == nullptr || !Target->GetOwnerActor()->HasAuthority()
		|| Target->HasMatchingGameplayTag(GolemGameplayTags::State_Life_Dead))
	{
		return;
	}
	const FGameplayEffectSpec& Spec = Parameters.GetOwningSpec();
	const float RawDamage = Spec.GetSetByCallerMagnitude(GolemGameplayTags::Data_Damage, false, 0.0f);
	if (!FMath::IsFinite(RawDamage) || RawDamage <= 0.0f)
	{
		return;
	}
	float Damage = FMath::Min(RawDamage, GolemMaxDamagePacket);
	const float RawPoiseDamage = Spec.GetSetByCallerMagnitude(GolemGameplayTags::Data_DamagePoise, false, 0.0f);
	const float PoiseDamage = FMath::IsFinite(RawPoiseDamage) ? FMath::Clamp(RawPoiseDamage, 0.0f, GolemMaxDamagePacket) : 0.0f;
	const bool bCanDefend = Spec.GetSetByCallerMagnitude(GolemGameplayTags::Data_DamageCanDefend, false, 0.0f) > 0.0f;
	if (bCanDefend || PoiseDamage > 0.0f)
	{
		if (IGolemDamageResponse* Response = FindDamageResponse(Target->GetOwnerActor()))
		{
			const float UnresolvedDamage = Damage;
			Response->ResolveIncomingDamage(Spec.GetContext().GetOriginalInstigator(), UnresolvedDamage,
				PoiseDamage, bCanDefend, Damage);
			// A response can mitigate damage; it cannot increase or inject a non-finite health mutation.
			Damage = FMath::IsFinite(Damage) ? FMath::Clamp(Damage, 0.0f, UnresolvedDamage) : UnresolvedDamage;
		}
	}
	if (Damage > 0.0f)
	{
		Output.AddOutputModifier(FGameplayModifierEvaluatedData(UGolemAttributeSet::GetIncomingDamageAttribute(),
			EGameplayModOp::Additive, Damage));
	}
}
