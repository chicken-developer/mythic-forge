#include "Definitions/KnightHeroDefinition.h"

#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "KnightHeroDefinition"

UKnightHeroDefinition::UKnightHeroDefinition()
{
	DefinitionType = TEXT("KnightHero");
}

FLinearColor UKnightHeroDefinition::GetBodyColor() const
{
	return BodyColor;
}

float UKnightHeroDefinition::GetWalkSpeed() const
{
	return WalkSpeed;
}

float UKnightHeroDefinition::GetSprintSpeed() const
{
	return SprintSpeed;
}

FVector UKnightHeroDefinition::GetFormationOffset() const
{
	return FormationOffset;
}

float UKnightHeroDefinition::GetFollowDistance() const
{
	return FollowDistance;
}

FPrimaryAssetId UKnightHeroDefinition::GetDefaultAbilitySetId() const
{
	return DefaultAbilitySetId;
}

FKnightResult UKnightHeroDefinition::ValidateHero() const
{
	const FKnightResult IdentityResult = ValidateIdentity();
	if (!IdentityResult.IsSuccess())
	{
		return IdentityResult;
	}
	if (GetDefinitionType() != FName(TEXT("KnightHero")))
	{
		return FKnightResult::Failure(EKnightError::TypeMismatch, GetDefinitionId(),
			LOCTEXT("WrongHeroType", "A hero definition must use the KnightHero primary asset type."));
	}
	if (!DefaultAbilitySetId.IsValid()
		|| DefaultAbilitySetId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightAbilitySet")))
	{
		return FKnightResult::Failure(EKnightError::InvalidId, GetDefinitionId(),
			LOCTEXT("InvalidAbilitySet", "A hero requires a valid KnightAbilitySet definition identity."));
	}
	if (!FMath::IsFinite(WalkSpeed) || !FMath::IsFinite(SprintSpeed)
		|| !FMath::IsFinite(FollowDistance) || WalkSpeed <= 0.0f || SprintSpeed < WalkSpeed
		|| FollowDistance <= 0.0f || FormationOffset.ContainsNaN()
		|| !FMath::IsFinite(BodyColor.R) || !FMath::IsFinite(BodyColor.G)
		|| !FMath::IsFinite(BodyColor.B) || !FMath::IsFinite(BodyColor.A))
	{
		return FKnightResult::Failure(EKnightError::InvalidData, GetDefinitionId(),
			LOCTEXT("InvalidHeroTuning", "Hero movement, formation and presentation tuning contains invalid values."));
	}
	return FKnightResult::Success();
}

FKnightResult UKnightHeroDefinition::TrySetDefaultAbilitySetId(const FPrimaryAssetId& InAbilitySetId)
{
	if (!InAbilitySetId.IsValid() || InAbilitySetId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightAbilitySet")))
	{
		return FKnightResult::Failure(EKnightError::InvalidId, GetDefinitionId(),
			LOCTEXT("InvalidAbilitySetAssignment",
				"The proposed ability set must use the KnightAbilitySet primary asset type."));
	}
	DefaultAbilitySetId = InAbilitySetId;
	return FKnightResult::Success();
}

FKnightResult UKnightHeroDefinition::TrySetPresentation(const FLinearColor& InBodyColor, float InWalkSpeed,
	float InSprintSpeed, const FVector& InFormationOffset, float InFollowDistance)
{
	if (!FMath::IsFinite(InWalkSpeed) || !FMath::IsFinite(InSprintSpeed)
		|| !FMath::IsFinite(InFollowDistance) || InWalkSpeed <= 0.0f || InSprintSpeed < InWalkSpeed
		|| InFollowDistance <= 0.0f || InFormationOffset.ContainsNaN()
		|| !FMath::IsFinite(InBodyColor.R) || !FMath::IsFinite(InBodyColor.G)
		|| !FMath::IsFinite(InBodyColor.B) || !FMath::IsFinite(InBodyColor.A))
	{
		return FKnightResult::Failure(EKnightError::InvalidData, GetDefinitionId(),
			LOCTEXT("InvalidPresentation", "The proposed hero tuning is invalid; the existing data was preserved."));
	}
	BodyColor = InBodyColor;
	WalkSpeed = InWalkSpeed;
	SprintSpeed = InSprintSpeed;
	FormationOffset = InFormationOffset;
	FollowDistance = InFollowDistance;
	return FKnightResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UKnightHeroDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FKnightResult Result = ValidateHero();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
