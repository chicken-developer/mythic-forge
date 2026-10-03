#include "Definitions/KnightCombatProfileDefinition.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/KnightAnimNotifyState_ComboWindow.h"
#include "Animation/KnightAnimNotifyState_TraceWindow.h"
#include "AnimNotifyState_MotionWarping.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/DataValidation.h"
#include "RootMotionModifier_SkewWarp.h"
#include "Tags/KnightGameplayTags.h"

namespace
{
	FKnightResult KnightInvalidCombatData(FName Context)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, Context,
			NSLOCTEXT("KnightCombat", "InvalidCombatContent", "Combat content does not satisfy its animation or tuning contract."));
	}
	bool KnightFiniteRange(float Value, float Minimum, float Maximum)
	{
		return FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum;
	}
	bool KnightValidMontage(const UAnimMontage* Montage, const USkeletalMesh* Mesh)
	{
		if (!IsValid(Montage) || Montage->GetPlayLength() <= 0.0f || Montage->GetSkeleton() == nullptr ||
			(Mesh != nullptr && Montage->GetSkeleton() != Mesh->GetSkeleton()))
		{
			return false;
		}
		for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
		{
			if (Track.SlotName == TEXT("DefaultSlot") && Track.AnimTrack.AnimSegments.Num() > 0)
			{
				return true;
			}
		}
		return false;
	}
	bool KnightValidTraversalMontage(const UAnimMontage* Montage)
	{
		if (Montage == nullptr || !Montage->HasRootMotion())
		{
			return false;
		}
		for (const FAnimNotifyEvent& Notify : Montage->Notifies)
		{
			const UAnimNotifyState_MotionWarping* Warping =
				Cast<UAnimNotifyState_MotionWarping>(Notify.NotifyStateClass);
			const URootMotionModifier_SkewWarp* Modifier = Warping != nullptr
				? Cast<URootMotionModifier_SkewWarp>(Warping->RootMotionModifier) : nullptr;
			if (Modifier != nullptr && Modifier->WarpTargetName == TEXT("KnightTraversal") &&
				Modifier->bWarpTranslation && Notify.GetDuration() > 0.0f && Notify.GetTriggerTime() >= 0.0f &&
				Notify.GetTriggerTime() + Notify.GetDuration() <= Montage->GetPlayLength() + KINDA_SMALL_NUMBER)
			{
				return true;
			}
		}
		return false;
	}
}

FKnightResult FKnightCombatTuning::Validate() const
{
	if (!KnightFiniteRange(MaxPoise, 1.0f, 10000.0f) || !KnightFiniteRange(StaggerDuration, 0.1f, 30.0f) ||
		!KnightFiniteRange(BlockFacingCosine, -1.0f, 1.0f) || !KnightFiniteRange(BlockDamageFraction, 0.0f, 1.0f) ||
		!KnightFiniteRange(BlockStaminaCost, 0.0f, 1000.0f) || !KnightFiniteRange(ParryWindow, 0.05f, 2.0f) ||
		!KnightFiniteRange(ParryStaminaCost, 0.0f, 1000.0f) || !KnightFiniteRange(DodgeDuration, 0.1f, 3.0f) ||
		!KnightFiniteRange(DodgeInvulnerabilityStart, 0.0f, DodgeDuration) ||
		!KnightFiniteRange(DodgeInvulnerabilityEnd, DodgeInvulnerabilityStart, DodgeDuration) ||
		!KnightFiniteRange(DodgeStaminaCost, 0.0f, 1000.0f) || !KnightFiniteRange(DodgeSpeed, 0.0f, 1500.0f) ||
		!KnightFiniteRange(LockRange, 100.0f, 5000.0f) || !KnightFiniteRange(RuneRange, 100.0f, 5000.0f) ||
		!KnightFiniteRange(RuneDamage, 1.0f, 10000.0f) || !KnightFiniteRange(RunePoiseDamage, 0.0f, 10000.0f) ||
		!KnightFiniteRange(RuneEnergyCost, 0.0f, 1000.0f) || !KnightFiniteRange(RuneSlowDuration, 0.1f, 30.0f))
	{
		return KnightInvalidCombatData(TEXT("CombatTuning"));
	}
	return FKnightResult::Success();
}

UKnightCombatProfileDefinition::UKnightCombatProfileDefinition()
{
	DefinitionType = TEXT("KnightCombatProfile");
}

USkeletalMesh* UKnightCombatProfileDefinition::GetSkeletalMesh() const
{
	return SkeletalMesh.LoadSynchronous();
}
UAnimSequence* UKnightCombatProfileDefinition::GetIdleAnimation() const
{
	return IdleAnimation.LoadSynchronous();
}
UAnimSequence* UKnightCombatProfileDefinition::GetWalkAnimation() const
{
	return WalkAnimation.LoadSynchronous();
}
UAnimMontage* UKnightCombatProfileDefinition::GetDodgeMontage() const
{
	return DodgeMontage.LoadSynchronous();
}
UAnimMontage* UKnightCombatProfileDefinition::GetRuneMontage() const
{
	return RuneMontage.LoadSynchronous();
}
UAnimMontage* UKnightCombatProfileDefinition::GetStaggerMontage() const
{
	return StaggerMontage.LoadSynchronous();
}
UAnimMontage* UKnightCombatProfileDefinition::GetTraversalMontage() const
{
	return TraversalMontage.LoadSynchronous();
}
UAnimMontage* UKnightCombatProfileDefinition::GetFinisherSourceMontage() const
{
	return FinisherSourceMontage.LoadSynchronous();
}
UAnimMontage* UKnightCombatProfileDefinition::GetFinisherTargetMontage() const
{
	return FinisherTargetMontage.LoadSynchronous();
}
const TArray<FKnightAttackDefinition>& UKnightCombatProfileDefinition::GetAttacks() const
{
	return Attacks;
}
const FKnightCombatTuning& UKnightCombatProfileDefinition::GetTuning() const
{
	return Tuning;
}

FPrimaryAssetId UKnightCombatProfileDefinition::GetCombatAbilitySetId() const
{
	return CombatAbilitySetId;
}

FKnightResult UKnightCombatProfileDefinition::TrySetCombatAbilitySetId(const FPrimaryAssetId& InAbilitySetId)
{
	if (!InAbilitySetId.IsValid() || InAbilitySetId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightAbilitySet")))
	{
		return KnightInvalidCombatData(TEXT("CombatAbilitySetId"));
	}
	CombatAbilitySetId = InAbilitySetId;
	return FKnightResult::Success();
}

FKnightResult UKnightCombatProfileDefinition::ValidateAttacks(const TArray<FKnightAttackDefinition>& InAttacks) const
{
	int32 LightCount = 0;
	int32 HeavyCount = 0;
	TSet<FString> SectionKeys;
	const UAnimMontage* PrimaryMontage = nullptr;
	float PreviousPrimarySectionStart = -1.0f;
	for (const FKnightAttackDefinition& Attack : InAttacks)
	{
		const UAnimMontage* Montage = Attack.Montage.LoadSynchronous();
		const bool bLight = Attack.ActionTag == KnightGameplayTags::Ability_Combat_Primary;
		if ((!bLight && Attack.ActionTag != KnightGameplayTags::Ability_Combat_Heavy) ||
			!KnightValidMontage(Montage, GetSkeletalMesh()) || Attack.SectionName.IsNone() ||
			!KnightFiniteRange(Attack.Damage, 0.01f, 10000.0f) ||
			!KnightFiniteRange(Attack.PoiseDamage, 0.0f, 10000.0f) ||
			!KnightFiniteRange(Attack.StaminaCost, 0.0f, 1000.0f) ||
			!KnightFiniteRange(Attack.TraceRadius, 1.0f, 100.0f))
		{
			return KnightInvalidCombatData(TEXT("AttackDefinition"));
		}
		const int32 SectionIndex = Montage->GetSectionIndex(Attack.SectionName);
		const FString Key = Montage->GetPathName() + TEXT(":") + Attack.SectionName.ToString();
		if (SectionIndex == INDEX_NONE || SectionKeys.Contains(Key))
		{
			return KnightInvalidCombatData(TEXT("AttackSection"));
		}
		SectionKeys.Add(Key);
		float SectionStart = 0.0f;
		float SectionEnd = 0.0f;
		Montage->GetSectionStartAndEndTime(SectionIndex, SectionStart, SectionEnd);
		if (bLight)
		{
			if ((PrimaryMontage != nullptr && PrimaryMontage != Montage) ||
				SectionStart <= PreviousPrimarySectionStart + KINDA_SMALL_NUMBER)
			{
				return KnightInvalidCombatData(TEXT("ComboMontageSectionOrder"));
			}
			PrimaryMontage = Montage;
			PreviousPrimarySectionStart = SectionStart;
		}
		bool bHasTrace = false;
		bool bHasCombo = false;
		for (const FAnimNotifyEvent& Notify : Montage->Notifies)
		{
			if (Notify.GetTriggerTime() < SectionStart - KINDA_SMALL_NUMBER ||
				Notify.GetTriggerTime() + Notify.GetDuration() > SectionEnd + KINDA_SMALL_NUMBER ||
				Notify.GetDuration() <= 0.0f)
			{
				continue;
			}
			if (const UKnightAnimNotifyState_TraceWindow* Trace =
				Cast<UKnightAnimNotifyState_TraceWindow>(Notify.NotifyStateClass))
			{
				bHasTrace |= !Trace->WindowId.IsNone();
			}
			bHasCombo |= Notify.NotifyStateClass != nullptr &&
				Notify.NotifyStateClass->IsA<UKnightAnimNotifyState_ComboWindow>();
		}
		if (!bHasTrace || (bLight && !bHasCombo))
		{
			return KnightInvalidCombatData(TEXT("AttackNotifyWindows"));
		}
		LightCount += bLight ? 1 : 0;
		HeavyCount += bLight ? 0 : 1;
	}
	return LightCount >= 2 && HeavyCount >= 1 ? FKnightResult::Success()
		: KnightInvalidCombatData(TEXT("CombatAttackCoverage"));
}

FKnightResult UKnightCombatProfileDefinition::ValidateCombatProfile() const
{
	const FKnightResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != TEXT("KnightCombatProfile"))
	{
		return KnightInvalidCombatData(TEXT("CombatProfileType"));
	}
	if (!CombatAbilitySetId.IsValid() ||
		CombatAbilitySetId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightAbilitySet")))
	{
		return KnightInvalidCombatData(TEXT("CombatAbilitySetId"));
	}
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const UAnimSequence* Idle = GetIdleAnimation();
	const UAnimSequence* Walk = GetWalkAnimation();
	if (!IsValid(Mesh) || !IsValid(Idle) || !IsValid(Walk) || Mesh->GetSkeleton() == nullptr ||
		Idle->GetSkeleton() != Mesh->GetSkeleton() || Walk->GetSkeleton() != Mesh->GetSkeleton() ||
		Mesh->GetRefSkeleton().FindBoneIndex(TEXT("hand_r")) == INDEX_NONE)
	{
		return KnightInvalidCombatData(TEXT("CombatSkeleton"));
	}
	const FKnightResult TuningResult = Tuning.Validate();
	if (!TuningResult.IsSuccess())
	{
		return TuningResult;
	}
	const FKnightResult AttackResult = ValidateAttacks(Attacks);
	if (!AttackResult.IsSuccess())
	{
		return AttackResult;
	}
	for (const UAnimMontage* Montage : {GetDodgeMontage(), GetRuneMontage(), GetStaggerMontage(),
		GetTraversalMontage(), GetFinisherSourceMontage(), GetFinisherTargetMontage()})
	{
		if (!KnightValidMontage(Montage, Mesh))
		{
			return KnightInvalidCombatData(TEXT("CombatActionMontages"));
		}
	}
	if (!KnightValidTraversalMontage(GetTraversalMontage()))
	{
		return KnightInvalidCombatData(TEXT("TraversalMotionWarpingContract"));
	}
	return FKnightResult::Success();
}

FKnightResult UKnightCombatProfileDefinition::TrySetPresentation(USkeletalMesh* Mesh,
	UAnimSequence* Idle, UAnimSequence* Walk)
{
	if (!IsValid(Mesh) || !IsValid(Idle) || !IsValid(Walk) || Mesh->GetSkeleton() == nullptr ||
		Idle->GetSkeleton() != Mesh->GetSkeleton() || Walk->GetSkeleton() != Mesh->GetSkeleton())
	{
		return KnightInvalidCombatData(TEXT("CombatPresentation"));
	}
	SkeletalMesh = Mesh;
	IdleAnimation = Idle;
	WalkAnimation = Walk;
	return FKnightResult::Success();
}

FKnightResult UKnightCombatProfileDefinition::TrySetAttacks(const TArray<FKnightAttackDefinition>& InAttacks)
{
	const FKnightResult Result = ValidateAttacks(InAttacks);
	if (Result.IsSuccess())
	{
		Attacks = InAttacks;
	}
	return Result;
}

FKnightResult UKnightCombatProfileDefinition::TrySetActionMontages(UAnimMontage* Dodge, UAnimMontage* Rune,
	UAnimMontage* Stagger, UAnimMontage* Traversal, UAnimMontage* FinisherSource, UAnimMontage* FinisherTarget)
{
	for (const UAnimMontage* Montage : {Dodge, Rune, Stagger, Traversal, FinisherSource, FinisherTarget})
	{
		if (!KnightValidMontage(Montage, GetSkeletalMesh()))
		{
			return KnightInvalidCombatData(TEXT("CombatActionMontages"));
		}
	}
	if (!KnightValidTraversalMontage(Traversal))
	{
		return KnightInvalidCombatData(TEXT("TraversalMotionWarpingContract"));
	}
	DodgeMontage = Dodge;
	RuneMontage = Rune;
	StaggerMontage = Stagger;
	TraversalMontage = Traversal;
	FinisherSourceMontage = FinisherSource;
	FinisherTargetMontage = FinisherTarget;
	return FKnightResult::Success();
}

FKnightResult UKnightCombatProfileDefinition::TrySetTuning(const FKnightCombatTuning& InTuning)
{
	const FKnightResult Result = InTuning.Validate();
	if (Result.IsSuccess())
	{
		Tuning = InTuning;
	}
	return Result;
}

#if WITH_EDITOR
EDataValidationResult UKnightCombatProfileDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const FKnightResult Result = ValidateCombatProfile();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Super::IsDataValid(Context);
}
#endif
