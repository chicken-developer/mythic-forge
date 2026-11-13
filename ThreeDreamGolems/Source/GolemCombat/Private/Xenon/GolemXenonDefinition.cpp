#include "Xenon/GolemXenonDefinition.h"

#include "Math/RandomStream.h"
#include "Misc/DataValidation.h"

namespace
{
	bool GolemXenonSlotValid(const FGolemXenonVoidSlot& Slot)
	{
		return !Slot.Offset.ContainsNaN() && Slot.Offset.Size() <= 1000.0 && FMath::IsFinite(Slot.Radius)
			&& Slot.Radius >= 5.0f && Slot.Radius <= 300.0f;
	}
}

FGolemResult FGolemXenonMove::Validate() const
{
	// 0.6 s is the shortest warning the design allows for any Xenon attack.
	if (!FMath::IsFinite(StartRange) || StartRange < 50.0f || StartRange > 5000.0f
		|| !FMath::IsFinite(TelegraphSeconds) || TelegraphSeconds < 0.6f || TelegraphSeconds > 30.0f
		|| !FMath::IsFinite(RecoverSeconds) || RecoverSeconds < 0.0f || RecoverSeconds > 60.0f
		|| !FMath::IsFinite(Range) || Range < StartRange || Range > 5000.0f
		|| !FMath::IsFinite(HalfAngleDegrees) || HalfAngleDegrees < 5.0f || HalfAngleDegrees > 180.0f
		|| !FMath::IsFinite(DamageScale) || DamageScale < 0.0f || DamageScale > 100.0f
		|| !FMath::IsFinite(Knockback) || Knockback < 0.0f || Knockback > 5000.0f)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("XenonMove"), FText::GetEmpty());
	}
	return FGolemResult::Success();
}

FGolemResult FGolemXenonBehavior::Validate() const
{
	if (!FMath::IsFinite(MoveSpeed) || MoveSpeed < 0.0f || MoveSpeed > 3000.0f
		|| !FMath::IsFinite(TurnRateDegrees) || TurnRateDegrees < 1.0f || TurnRateDegrees > 3600.0f
		|| !FMath::IsFinite(AggroRange) || AggroRange < 100.0f || AggroRange > 100000.0f || Moves.Num() > 8)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("XenonBehavior"), FText::GetEmpty());
	}
	for (const FGolemXenonMove& Move : Moves)
	{
		const FGolemResult Result = Move.Validate();
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	return FGolemResult::Success();
}

UGolemXenonDefinition::UGolemXenonDefinition()
{
	DefinitionType = TEXT("GolemXenon");
	VoidSlots.SetNum(2);
	VoidSlots[0].Offset = FVector(45.0, 0.0, 30.0);
	VoidSlots[1].Offset = FVector(-45.0, 0.0, 30.0);
	CoreSlot.Offset = FVector(0.0, 0.0, 95.0);
}

FGolemResult UGolemXenonDefinition::ValidateXenon() const
{
	const FGolemResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != FName(TEXT("GolemXenon")) ||
		!FMath::IsFinite(MaxHealth) || MaxHealth < 1.0f || MaxHealth > 1000000.0f ||
		!FMath::IsFinite(ContactDamage) || ContactDamage < 0.0f || ContactDamage > 1000000.0f ||
		!FMath::IsFinite(BodyScale) || BodyScale < 0.5f || BodyScale > 6.0f ||
		!FMath::IsFinite(MixedVoidChance) || MixedVoidChance < 0.0f || MixedVoidChance > 1.0f ||
		VoidSlots.Num() < Voids.MaxOpenVoids || VoidSlots.Num() > MaxVoidSlots || !GolemXenonSlotValid(CoreSlot))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("XenonDefinition"), FText::GetEmpty());
	}
	for (const FGolemXenonVoidSlot& Slot : VoidSlots)
	{
		if (!GolemXenonSlotValid(Slot))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("XenonVoidSlot"), FText::GetEmpty());
		}
	}
	const FGolemResult BehaviorResult = Behavior.Validate();
	return BehaviorResult.IsSuccess() ? Voids.Validate() : BehaviorResult;
}

FGolemResult UGolemXenonDefinition::TrySetBehavior(const FGolemXenonBehavior& InBehavior)
{
	const FGolemResult Result = InBehavior.Validate();
	if (Result.IsSuccess())
	{
		Behavior = InBehavior;
	}
	return Result;
}

TArray<uint8> UGolemXenonDefinition::BuildColorPlan(const int32 Seed, const float MixedChanceOverride) const
{
	static const uint8 Basic[] = {
		GolemElements::Mask(EGolemElement::Earth), GolemElements::Mask(EGolemElement::Sky), GolemElements::Mask(EGolemElement::Sea) };
	static const uint8 Mixed[] = {
		GolemElements::Mask(EGolemElement::Earth, EGolemElement::Sky), GolemElements::Mask(EGolemElement::Sky, EGolemElement::Sea),
		GolemElements::Mask(EGolemElement::Earth, EGolemElement::Sea) };
	const float Chance = MixedChanceOverride >= 0.0f ? FMath::Clamp(MixedChanceOverride, 0.0f, 1.0f) : MixedVoidChance;
	FRandomStream Stream(Seed);
	TArray<uint8> Plan;
	Plan.Reserve(Voids.VoidsToFill);
	for (int32 Index = 0; Index < Voids.VoidsToFill; ++Index)
	{
		// Both draws happen for every void so the plan of void N never depends on earlier options.
		const float Roll = Stream.GetFraction();
		const int32 Pick = Stream.RandRange(0, 2);
		const bool bMixed = Roll < Chance && !(Index == 0 && bFirstVoidBasic);
		Plan.Add(bMixed ? Mixed[Pick] : Basic[Pick]);
	}
	return Plan;
}

FGolemResult UGolemXenonDefinition::TrySetXenon(const float InMaxHealth, const float InContactDamage, const float InBodyScale,
	const FGolemVoidHostTuning& InVoids, const TArray<FGolemXenonVoidSlot>& InSlots, const float InMixedVoidChance,
	const bool bInFirstVoidBasic)
{
	UGolemXenonDefinition* Candidate = DuplicateObject<UGolemXenonDefinition>(this, GetTransientPackage());
	Candidate->MaxHealth = InMaxHealth;
	Candidate->ContactDamage = InContactDamage;
	Candidate->BodyScale = InBodyScale;
	Candidate->Voids = InVoids;
	Candidate->VoidSlots = InSlots;
	Candidate->MixedVoidChance = InMixedVoidChance;
	Candidate->bFirstVoidBasic = bInFirstVoidBasic;
	const FGolemResult Result = Candidate->ValidateXenon();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	MaxHealth = InMaxHealth;
	ContactDamage = InContactDamage;
	BodyScale = InBodyScale;
	Voids = InVoids;
	VoidSlots = InSlots;
	MixedVoidChance = InMixedVoidChance;
	bFirstVoidBasic = bInFirstVoidBasic;
	return FGolemResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UGolemXenonDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const FGolemResult Result = ValidateXenon();
	if (!Result.IsSuccess())
	{
		Context.AddError(FText::FromName(Result.Context));
		return EDataValidationResult::Invalid;
	}
	return Super::IsDataValid(Context);
}
#endif
