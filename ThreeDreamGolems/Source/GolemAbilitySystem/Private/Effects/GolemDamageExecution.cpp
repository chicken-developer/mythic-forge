#include "Effects/GolemDamageExecution.h"

#include "Attributes/GolemAttributeSet.h"
#include "Components/ActorComponent.h"
#include "Damage/GolemDamageResponse.h"
#include "Damage/GolemArmorResponse.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "GameFramework/Actor.h"
#include "Tags/GolemGameplayTags.h"

namespace
{
	struct FGolemDamageCaptures
	{
		FGameplayEffectAttributeCaptureDefinition DefenseDef;

		FGolemDamageCaptures()
		{
			DefenseDef = FGameplayEffectAttributeCaptureDefinition(UGolemAttributeSet::GetDefenseAttribute(),
				EGameplayEffectAttributeCaptureSource::Target, false);
		}
	};

	const FGolemDamageCaptures& DamageCaptures()
	{
		static const FGolemDamageCaptures Captures;
		return Captures;
	}
}

UGolemDamageExecution::UGolemDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageCaptures().DefenseDef);
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
	const bool bItemProc = Spec.GetSetByCallerMagnitude(FName(TEXT("ItemProc")), false, 0.0f) > 0.0f;
	UGolemSkillRuntimeComponent* SourceRuntime = Parameters.GetSourceAbilitySystemComponent() != nullptr
		&& Parameters.GetSourceAbilitySystemComponent()->GetOwnerActor() != nullptr
		? Parameters.GetSourceAbilitySystemComponent()->GetOwnerActor()->FindComponentByClass<UGolemSkillRuntimeComponent>() : nullptr;
	const uint64 SourceItemGeneration = SourceRuntime != nullptr ? SourceRuntime->GetItemGeneration() : 0;
	uint32 ActivationParts[4] = {};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		ActivationParts[Index] = static_cast<uint32>(Spec.GetSetByCallerMagnitude(
			FName(*FString::Printf(TEXT("SkillActivation%dLow"), Index)), false, 0.0f));
		ActivationParts[Index] |= static_cast<uint32>(Spec.GetSetByCallerMagnitude(
			FName(*FString::Printf(TEXT("SkillActivation%dHigh"), Index)), false, 0.0f)) << 16;
	}
	const FGuid ActivationId(ActivationParts[0], ActivationParts[1], ActivationParts[2], ActivationParts[3]);
	const float RawDamage = Spec.GetSetByCallerMagnitude(GolemGameplayTags::Data_Damage, false, 0.0f);
	if (!FMath::IsFinite(RawDamage) || RawDamage <= 0.0f)
	{
		return;
	}
	FAggregatorEvaluateParameters Evaluation;
	Evaluation.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Evaluation.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	float Defense = 0.0f;
	Parameters.AttemptCalculateCapturedAttributeMagnitude(DamageCaptures().DefenseDef, Evaluation, Defense);
	const bool bSeed = Spec.GetSetByCallerMagnitude(FName(TEXT("SkillSeed")), false, 0.0f) > 0.0f;
	UGolemSkillRuntimeComponent* Runtime = Target->GetOwnerActor()->FindComponentByClass<UGolemSkillRuntimeComponent>();
	const uint64 TargetItemGeneration = Runtime != nullptr ? Runtime->GetItemGeneration() : 0;
	float ArmorAdjustedDamage = RawDamage;
	if (const FHitResult* Hit = bItemProc ? nullptr : Spec.GetContext().GetHitResult())
	{
		IGolemArmorResponse* Armor = Cast<IGolemArmorResponse>(Target->GetOwnerActor());
		if (Armor == nullptr)
		{
			TInlineComponentArray<UActorComponent*> Components(Target->GetOwnerActor());
			for (UActorComponent* Component : Components)
			{
				Armor = Cast<IGolemArmorResponse>(Component);
				if (Armor != nullptr)
				{
					break;
				}
			}
		}
		if (Armor != nullptr)
		{
			Armor->ResolveArmorHit(*Hit, ActivationId,
				static_cast<int32>(Spec.GetSetByCallerMagnitude(FName(TEXT("SkillHitIndex")), false, 0.0f)),
				Spec.GetSetByCallerMagnitude(FName(TEXT("SkillHeavy")), false, 0.0f) > 0.0f,
				RawDamage, ArmorAdjustedDamage);
			if (ArmorAdjustedDamage == 0.0f)
			{
				// A duplicate armor-zone hit suppresses the entire positive-damage packet, including Poise.
				return;
			}
		}
	}
	float Damage = FMath::Max(0.0f, ArmorAdjustedDamage - FMath::Max(0.0f, Defense));
	if (bSeed)
	{
		const UGolemAttributeSet* A = Cast<UGolemAttributeSet>(Target->GetAttributeSet(UGolemAttributeSet::StaticClass()));
		if (A == nullptr)
		{
			return;
		}
		const EGolemSkillSchool School = static_cast<EGolemSkillSchool>(FMath::Clamp(
			static_cast<int32>(Spec.GetSetByCallerMagnitude(FName(TEXT("SkillSchool")), false, 0.0f)), 0, 4));
		float Resistance = 0.0f;
		switch (School)
		{
		case EGolemSkillSchool::Arcane:
			Resistance = A->GetArcaneResistance();
			break;
		case EGolemSkillSchool::Lightning:
			Resistance = A->GetLightningResistance();
			break;
		case EGolemSkillSchool::Frost:
			Resistance = A->GetFrostResistance();
			break;
		case EGolemSkillSchool::Fire:
			Resistance = A->GetFireResistance();
			break;
		default:
			break;
		}
		const float Reduction = A->GetDamageReduction() + (Runtime != nullptr ? Runtime->GetIncomingReduction(
			Spec.GetContext().GetOriginalInstigator()) : 0.0f);
		Damage = FGolemSkillFormula::Mitigate(ArmorAdjustedDamage, School, Defense, Resistance, Reduction,
			A->GetVersatility(), Spec.GetSetByCallerMagnitude(FName(TEXT("SkillFrostPenetration")), false, 0.0f));
	}
	const float RawPoiseDamage = Spec.GetSetByCallerMagnitude(GolemGameplayTags::Data_DamagePoise, false, 0.0f);
	const float PoiseDamage = (!bItemProc && FMath::IsFinite(RawPoiseDamage)
		? FMath::Clamp(RawPoiseDamage, 0.0f, 1000000.0f) : 0.0f)
		* (Runtime != nullptr ? Runtime->GetPoiseScale() : 1.0f);
	const bool bCanDefend = !bItemProc
		&& Spec.GetSetByCallerMagnitude(GolemGameplayTags::Data_DamageCanDefend, false, 0.0f) > 0.0f;
	if ((bCanDefend || PoiseDamage > 0.0f) && FMath::IsFinite(Damage))
	{
		AActor* TargetActor = Target->GetOwnerActor();
		IGolemDamageResponse* Response = Cast<IGolemDamageResponse>(TargetActor);
		if (Response == nullptr)
		{
			TInlineComponentArray<UActorComponent*> Components(TargetActor);
			for (UActorComponent* Component : Components)
			{
				if (IsValid(Component))
				{
					Response = Cast<IGolemDamageResponse>(Component);
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
			FGolemIncomingHitContext Context;
			Context.ActivationId = ActivationId;
			Context.bDirect = !bItemProc && Spec.GetSetByCallerMagnitude(FName(TEXT("SkillDirect")), false,
				bCanDefend ? 1.0f : 0.0f) > 0.0f;
			Context.TargetItemGeneration = TargetItemGeneration;
			Response->ResolveIncomingDamageWithContext(Spec.GetContext().GetOriginalInstigator(), UnresolvedDamage,
				PoiseDamage, bCanDefend, Context, Damage);
			// A response can mitigate damage; it cannot increase or inject a non-finite health mutation.
			Damage = FMath::IsFinite(Damage) ? FMath::Clamp(Damage, 0.0f, UnresolvedDamage) : UnresolvedDamage;
		}
	}
	if (!bItemProc && SourceRuntime != nullptr && Damage > 0.0f
		&& Spec.GetSetByCallerMagnitude(FName(TEXT("ItemEligibleActivation")), false, 0.0f) > 0.0f
		&& Spec.GetSetByCallerMagnitude(FName(TEXT("SkillDirect")), false, bCanDefend ? 1.0f : 0.0f) > 0.0f)
	{
		SourceRuntime->NotifyConfirmedDirectDamage(Target->GetOwnerActor(), ActivationId,
			static_cast<int32>(Spec.GetSetByCallerMagnitude(FName(TEXT("SkillHitIndex")), false, 0.0f)),
			static_cast<EGolemSkillSchool>(FMath::Clamp(static_cast<int32>(
				Spec.GetSetByCallerMagnitude(FName(TEXT("SkillSchool")), false, 0.0f)), 0, 4)), Damage,
			Spec.GetContext().Get()->GetActors().IsEmpty() ? nullptr : Spec.GetContext().Get()->GetActors()[0].Get(),
			SourceItemGeneration);
	}
	if (Runtime != nullptr && Damage > 0.0f)
	{
		Damage = Runtime->ResolveDamagePools(Damage,
			Spec.GetSetByCallerMagnitude(FName(TEXT("SkillDeferred")), false, bCanDefend ? 1.0f : 0.0f) > 0.0f);
	}
	if (Damage > 0.0f && FMath::IsFinite(Damage))
	{
		Output.AddOutputModifier(FGameplayModifierEvaluatedData(UGolemAttributeSet::GetIncomingDamageAttribute(),
			EGameplayModOp::Additive, Damage));
	}
}
