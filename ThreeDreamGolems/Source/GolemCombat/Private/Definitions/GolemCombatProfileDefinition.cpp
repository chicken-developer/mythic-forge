#include "Definitions/GolemCombatProfileDefinition.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/GolemAnimNotifyState_ComboWindow.h"
#include "Animation/GolemAnimNotifyState_TraceWindow.h"
#include "AnimNotifyState_MotionWarping.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/DataValidation.h"
#include "RootMotionModifier_SkewWarp.h"
#include "Tags/GolemGameplayTags.h"

namespace
{
	FGolemResult GolemInvalidCombatData(FName Context)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, Context,
			NSLOCTEXT("GolemCombat", "InvalidCombatContent", "Combat content does not satisfy its animation or tuning contract."));
	}
	bool GolemFiniteRange(float Value, float Minimum, float Maximum)
	{
		return FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum;
	}
	bool GolemValidMontage(const UAnimMontage* Montage, const USkeletalMesh* Mesh)
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
	bool GolemValidTraversalMontage(const UAnimMontage* Montage)
	{
		if (Montage == nullptr || !Montage->HasRootMotion())
		{
			return false;
		}
		const float BlendOutStart = Montage->GetPlayLength() - (Montage->BlendOutTriggerTime < 0.0f
			? Montage->BlendOut.GetBlendTime() : Montage->BlendOutTriggerTime);
		if (!FMath::IsFinite(BlendOutStart))
		{
			return false;
		}
		for (const FAnimNotifyEvent& Notify : Montage->Notifies)
		{
			const UAnimNotifyState_MotionWarping* Warping =
				Cast<UAnimNotifyState_MotionWarping>(Notify.NotifyStateClass);
			const URootMotionModifier_SkewWarp* Modifier = Warping != nullptr
				? Cast<URootMotionModifier_SkewWarp>(Warping->RootMotionModifier) : nullptr;
			if (Modifier != nullptr && Modifier->WarpTargetName == TEXT("GolemTraversal") &&
				Modifier->bWarpTranslation && Notify.GetDuration() > 0.0f && Notify.GetTriggerTime() >= 0.0f &&
				Notify.GetTriggerTime() + Notify.GetDuration() <= Montage->GetPlayLength() + KINDA_SMALL_NUMBER &&
				Notify.GetTriggerTime() + Notify.GetDuration() <= BlendOutStart + KINDA_SMALL_NUMBER)
			{
				return true;
			}
		}
		return false;
	}
}

FGolemResult FGolemHeroMoveTuning::Validate() const
{
	if (!GolemFiniteRange(HeavyChargeSeconds, 0.0f, 10.0f) || !GolemFiniteRange(ProjectileRange, 100.0f, 20000.0f) ||
		!GolemFiniteRange(ProjectileSpeed, 100.0f, 20000.0f) || !GolemFiniteRange(ProjectileRadius, 1.0f, 200.0f) ||
		!GolemFiniteRange(ClassMoveCooldownSeconds, 0.0f, 600.0f) || !GolemFiniteRange(GuardDamageFraction, 0.0f, 1.0f) ||
		!GolemFiniteRange(GuardMaxSeconds, 0.0f, 600.0f) || !GolemFiniteRange(BackflipDistance, 0.0f, 3000.0f) ||
		!GolemFiniteRange(BackflipShotPowerScale, 0.0f, 100.0f) || !GolemFiniteRange(BarrierRadius, 0.0f, 10000.0f) ||
		!GolemFiniteRange(BarrierSeconds, 0.1f, 600.0f))
	{
		return GolemInvalidCombatData(TEXT("HeroMoveTuning"));
	}
	return FGolemResult::Success();
}

FGolemResult FGolemCombatTuning::Validate() const
{
	if (!GolemFiniteRange(MaxPoise, 1.0f, 10000.0f) || !GolemFiniteRange(StaggerDuration, 0.1f, 30.0f) ||
		!GolemFiniteRange(BlockFacingCosine, -1.0f, 1.0f) ||
		!GolemFiniteRange(ParryWindow, 0.05f, 2.0f) ||
		!GolemFiniteRange(DodgeDuration, 0.1f, 3.0f) ||
		!GolemFiniteRange(DodgeInvulnerabilityStart, 0.0f, DodgeDuration) ||
		!GolemFiniteRange(DodgeInvulnerabilityEnd, DodgeInvulnerabilityStart, DodgeDuration) ||
		!GolemFiniteRange(DodgeCooldownSeconds, 0.0f, 60.0f) ||
		!GolemFiniteRange(DodgeSpeed, 0.0f, 1500.0f) ||
		!GolemFiniteRange(LockRange, 100.0f, 5000.0f))
	{
		return GolemInvalidCombatData(TEXT("CombatTuning"));
	}
	return FGolemResult::Success();
}

UGolemCombatProfileDefinition::UGolemCombatProfileDefinition()
{
	DefinitionType = TEXT("GolemCombatProfile");
}

USkeletalMesh* UGolemCombatProfileDefinition::GetSkeletalMesh() const
{
	return SkeletalMesh.LoadSynchronous();
}
UAnimSequence* UGolemCombatProfileDefinition::GetIdleAnimation() const
{
	return IdleAnimation.LoadSynchronous();
}
UAnimSequence* UGolemCombatProfileDefinition::GetWalkAnimation() const
{
	return WalkAnimation.LoadSynchronous();
}
UAnimMontage* UGolemCombatProfileDefinition::GetDodgeMontage() const
{
	return DodgeMontage.LoadSynchronous();
}
UAnimMontage* UGolemCombatProfileDefinition::GetStaggerMontage() const
{
	return StaggerMontage.LoadSynchronous();
}
UAnimMontage* UGolemCombatProfileDefinition::GetTraversalMontage() const
{
	return TraversalMontage.LoadSynchronous();
}
UAnimMontage* UGolemCombatProfileDefinition::GetFinisherSourceMontage() const
{
	return FinisherSourceMontage.LoadSynchronous();
}
UAnimMontage* UGolemCombatProfileDefinition::GetFinisherTargetMontage() const
{
	return FinisherTargetMontage.LoadSynchronous();
}
const TArray<FGolemAttackDefinition>& UGolemCombatProfileDefinition::GetAttacks() const
{
	return Attacks;
}
const FGolemCombatTuning& UGolemCombatProfileDefinition::GetTuning() const
{
	return Tuning;
}

FPrimaryAssetId UGolemCombatProfileDefinition::GetCombatAbilitySetId() const
{
	return CombatAbilitySetId;
}

FGolemResult UGolemCombatProfileDefinition::TrySetCombatAbilitySetId(const FPrimaryAssetId& InAbilitySetId)
{
	if (!InAbilitySetId.IsValid() || InAbilitySetId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemAbilitySet")))
	{
		return GolemInvalidCombatData(TEXT("CombatAbilitySetId"));
	}
	CombatAbilitySetId = InAbilitySetId;
	return FGolemResult::Success();
}

FGolemResult UGolemCombatProfileDefinition::ValidateAttacks(const TArray<FGolemAttackDefinition>& InAttacks) const
{
	int32 LightCount = 0;
	int32 HeavyCount = 0;
	TSet<FString> SectionKeys;
	const UAnimMontage* PrimaryMontage = nullptr;
	float PreviousPrimarySectionStart = -1.0f;
	for (const FGolemAttackDefinition& Attack : InAttacks)
	{
		const UAnimMontage* Montage = Attack.Montage.LoadSynchronous();
		const bool bLight = Attack.ActionTag == GolemGameplayTags::Ability_Combat_Primary;
		if ((!bLight && Attack.ActionTag != GolemGameplayTags::Ability_Combat_Heavy) ||
			!GolemValidMontage(Montage, GetSkeletalMesh()) || Attack.SectionName.IsNone() ||
			!GolemFiniteRange(Attack.PowerScale, 0.01f, 100.0f) ||
			!GolemFiniteRange(Attack.PoiseDamage, 0.0f, 10000.0f) ||
			
			!GolemFiniteRange(Attack.TraceRadius, 1.0f, 100.0f))
		{
			return GolemInvalidCombatData(TEXT("AttackDefinition"));
		}
		const int32 SectionIndex = Montage->GetSectionIndex(Attack.SectionName);
		const FString Key = Montage->GetPathName() + TEXT(":") + Attack.SectionName.ToString();
		if (SectionIndex == INDEX_NONE || SectionKeys.Contains(Key))
		{
			return GolemInvalidCombatData(TEXT("AttackSection"));
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
				return GolemInvalidCombatData(TEXT("ComboMontageSectionOrder"));
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
			if (const UGolemAnimNotifyState_TraceWindow* Trace =
				Cast<UGolemAnimNotifyState_TraceWindow>(Notify.NotifyStateClass))
			{
				bHasTrace |= !Trace->WindowId.IsNone();
			}
			bHasCombo |= Notify.NotifyStateClass != nullptr &&
				Notify.NotifyStateClass->IsA<UGolemAnimNotifyState_ComboWindow>();
		}
		if (!bHasTrace || (bLight && !bHasCombo))
		{
			return GolemInvalidCombatData(TEXT("AttackNotifyWindows"));
		}
		LightCount += bLight ? 1 : 0;
		HeavyCount += bLight ? 0 : 1;
	}
	return LightCount >= 2 && HeavyCount >= 1 ? FGolemResult::Success()
		: GolemInvalidCombatData(TEXT("CombatAttackCoverage"));
}

FGolemResult UGolemCombatProfileDefinition::ValidateCombatProfile() const
{
	const FGolemResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != TEXT("GolemCombatProfile"))
	{
		return GolemInvalidCombatData(TEXT("CombatProfileType"));
	}
	if (!CombatAbilitySetId.IsValid() ||
		CombatAbilitySetId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemAbilitySet")))
	{
		return GolemInvalidCombatData(TEXT("CombatAbilitySetId"));
	}
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const UAnimSequence* Idle = GetIdleAnimation();
	const UAnimSequence* Walk = GetWalkAnimation();
	if (!IsValid(Mesh) || !IsValid(Idle) || !IsValid(Walk) || Mesh->GetSkeleton() == nullptr ||
		Idle->GetSkeleton() != Mesh->GetSkeleton() || Walk->GetSkeleton() != Mesh->GetSkeleton() ||
		Mesh->GetRefSkeleton().FindBoneIndex(TEXT("hand_r")) == INDEX_NONE)
	{
		return GolemInvalidCombatData(TEXT("CombatSkeleton"));
	}
	const FGolemResult TuningResult = Tuning.Validate();
	if (!TuningResult.IsSuccess())
	{
		return TuningResult;
	}
	const FGolemResult AttackResult = ValidateAttacks(Attacks);
	if (!AttackResult.IsSuccess())
	{
		return AttackResult;
	}
	for (const UAnimMontage* Montage : {GetDodgeMontage(), GetStaggerMontage(),
		GetTraversalMontage(), GetFinisherSourceMontage(), GetFinisherTargetMontage()})
	{
		if (!GolemValidMontage(Montage, Mesh))
		{
			return GolemInvalidCombatData(TEXT("CombatActionMontages"));
		}
	}
	if (!GolemValidTraversalMontage(GetTraversalMontage()))
	{
		return GolemInvalidCombatData(TEXT("TraversalMotionWarpingContract"));
	}
	return FGolemResult::Success();
}

FGolemResult UGolemCombatProfileDefinition::TrySetPresentation(USkeletalMesh* Mesh,
	UAnimSequence* Idle, UAnimSequence* Walk)
{
	if (!IsValid(Mesh) || !IsValid(Idle) || !IsValid(Walk) || Mesh->GetSkeleton() == nullptr ||
		Idle->GetSkeleton() != Mesh->GetSkeleton() || Walk->GetSkeleton() != Mesh->GetSkeleton())
	{
		return GolemInvalidCombatData(TEXT("CombatPresentation"));
	}
	SkeletalMesh = Mesh;
	IdleAnimation = Idle;
	WalkAnimation = Walk;
	return FGolemResult::Success();
}

FGolemResult UGolemCombatProfileDefinition::TrySetAttacks(const TArray<FGolemAttackDefinition>& InAttacks)
{
	const FGolemResult Result = ValidateAttacks(InAttacks);
	if (Result.IsSuccess())
	{
		Attacks = InAttacks;
	}
	return Result;
}

FGolemResult UGolemCombatProfileDefinition::TrySetActionMontages(UAnimMontage* Dodge,
	UAnimMontage* Stagger, UAnimMontage* Traversal, UAnimMontage* FinisherSource, UAnimMontage* FinisherTarget)
{
	for (const UAnimMontage* Montage : {Dodge, Stagger, Traversal, FinisherSource, FinisherTarget})
	{
		if (!GolemValidMontage(Montage, GetSkeletalMesh()))
		{
			return GolemInvalidCombatData(TEXT("CombatActionMontages"));
		}
	}
	if (!GolemValidTraversalMontage(Traversal))
	{
		return GolemInvalidCombatData(TEXT("TraversalMotionWarpingContract"));
	}
	DodgeMontage = Dodge;
	StaggerMontage = Stagger;
	TraversalMontage = Traversal;
	FinisherSourceMontage = FinisherSource;
	FinisherTargetMontage = FinisherTarget;
	return FGolemResult::Success();
}

FGolemResult UGolemCombatProfileDefinition::TrySetTuning(const FGolemCombatTuning& InTuning)
{
	const FGolemResult Result = InTuning.Validate();
	if (Result.IsSuccess())
	{
		Tuning = InTuning;
	}
	return Result;
}

#if WITH_EDITOR
EDataValidationResult UGolemCombatProfileDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const FGolemResult Result = ValidateCombatProfile();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Super::IsDataValid(Context);
}
#endif
