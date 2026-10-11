#include "Effects/KnightDamageExecution.h"

#include "Attributes/KnightAttributeSet.h"
#include "Components/ActorComponent.h"
#include "Damage/KnightDamageResponse.h"
#include "Damage/KnightArmorResponse.h"
#include "Skills/KnightSkillRuntimeComponent.h"
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
	const bool bItemProc = Spec.GetSetByCallerMagnitude(FName(TEXT("ItemProc")), false, 0.0f) > 0.0f;
	UKnightSkillRuntimeComponent* SourceRuntime = Parameters.GetSourceAbilitySystemComponent() != nullptr
		&& Parameters.GetSourceAbilitySystemComponent()->GetOwnerActor() != nullptr
		? Parameters.GetSourceAbilitySystemComponent()->GetOwnerActor()->FindComponentByClass<UKnightSkillRuntimeComponent>() : nullptr;
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
	const bool bSeed = Spec.GetSetByCallerMagnitude(FName(TEXT("SkillSeed")), false, 0.0f) > 0.0f;
	UKnightSkillRuntimeComponent* Runtime = Target->GetOwnerActor()->FindComponentByClass<UKnightSkillRuntimeComponent>();
	const uint64 TargetItemGeneration = Runtime != nullptr ? Runtime->GetItemGeneration() : 0;
	float ArmorAdjustedDamage = RawDamage;
	if (const FHitResult* Hit = bItemProc ? nullptr : Spec.GetContext().GetHitResult())
	{
		IKnightArmorResponse* Armor = Cast<IKnightArmorResponse>(Target->GetOwnerActor());
		if (Armor == nullptr)
		{
			TInlineComponentArray<UActorComponent*> Components(Target->GetOwnerActor());
			for (UActorComponent* Component : Components)
			{
				Armor = Cast<IKnightArmorResponse>(Component);
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
		const UKnightAttributeSet* A = Cast<UKnightAttributeSet>(Target->GetAttributeSet(UKnightAttributeSet::StaticClass()));
		if (A == nullptr)
		{
			return;
		}
		const EKnightSkillSchool School = static_cast<EKnightSkillSchool>(FMath::Clamp(
			static_cast<int32>(Spec.GetSetByCallerMagnitude(FName(TEXT("SkillSchool")), false, 0.0f)), 0, 4));
		float Resistance = 0.0f;
		switch (School)
		{
		case EKnightSkillSchool::Arcane:
			Resistance = A->GetArcaneResistance();
			break;
		case EKnightSkillSchool::Lightning:
			Resistance = A->GetLightningResistance();
			break;
		case EKnightSkillSchool::Frost:
			Resistance = A->GetFrostResistance();
			break;
		case EKnightSkillSchool::Fire:
			Resistance = A->GetFireResistance();
			break;
		default:
			break;
		}
		const float Reduction = A->GetDamageReduction() + (Runtime != nullptr ? Runtime->GetIncomingReduction(
			Spec.GetContext().GetOriginalInstigator()) : 0.0f);
		Damage = FKnightSkillFormula::Mitigate(ArmorAdjustedDamage, School, Defense, Resistance, Reduction,
			A->GetVersatility(), Spec.GetSetByCallerMagnitude(FName(TEXT("SkillFrostPenetration")), false, 0.0f));
	}
	const float RawPoiseDamage = Spec.GetSetByCallerMagnitude(KnightGameplayTags::Data_DamagePoise, false, 0.0f);
	const float PoiseDamage = (!bItemProc && FMath::IsFinite(RawPoiseDamage)
		? FMath::Clamp(RawPoiseDamage, 0.0f, 1000000.0f) : 0.0f)
		* (Runtime != nullptr ? Runtime->GetPoiseScale() : 1.0f);
	const bool bCanDefend = !bItemProc
		&& Spec.GetSetByCallerMagnitude(KnightGameplayTags::Data_DamageCanDefend, false, 0.0f) > 0.0f;
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
			FKnightIncomingHitContext Context;
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
			static_cast<EKnightSkillSchool>(FMath::Clamp(static_cast<int32>(
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
		Output.AddOutputModifier(FGameplayModifierEvaluatedData(UKnightAttributeSet::GetIncomingDamageAttribute(),
			EGameplayModOp::Additive, Damage));
	}
}
