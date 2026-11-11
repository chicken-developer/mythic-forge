#include "Definitions/GolemHeroDefinition.h"

#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "GolemHeroDefinition"

UGolemHeroDefinition::UGolemHeroDefinition()
{
	DefinitionType = TEXT("GolemHero");
}

FLinearColor UGolemHeroDefinition::GetBodyColor() const
{
	return BodyColor;
}

float UGolemHeroDefinition::GetWalkSpeed() const
{
	return WalkSpeed;
}

float UGolemHeroDefinition::GetSprintSpeed() const
{
	return SprintSpeed;
}

FVector UGolemHeroDefinition::GetFormationOffset() const
{
	return FormationOffset;
}

float UGolemHeroDefinition::GetFollowDistance() const
{
	return FollowDistance;
}

FPrimaryAssetId UGolemHeroDefinition::GetDefaultAbilitySetId() const
{
	return DefaultAbilitySetId;
}

FGolemResult UGolemHeroDefinition::ValidateHero() const
{
	const FGolemResult IdentityResult = ValidateIdentity();
	if (!IdentityResult.IsSuccess())
	{
		return IdentityResult;
	}
	if (GetDefinitionType() != FName(TEXT("GolemHero")))
	{
		return FGolemResult::Failure(EGolemError::TypeMismatch, GetDefinitionId(),
			LOCTEXT("WrongHeroType", "A hero definition must use the GolemHero primary asset type."));
	}
	if (!DefaultAbilitySetId.IsValid()
		|| DefaultAbilitySetId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemAbilitySet")))
	{
		return FGolemResult::Failure(EGolemError::InvalidId, GetDefinitionId(),
			LOCTEXT("InvalidAbilitySet", "A hero requires a valid GolemAbilitySet definition identity."));
	}
	if (!FMath::IsFinite(WalkSpeed) || !FMath::IsFinite(SprintSpeed)
		|| !FMath::IsFinite(FollowDistance) || WalkSpeed <= 0.0f || SprintSpeed < WalkSpeed
		|| FollowDistance <= 0.0f || FormationOffset.ContainsNaN()
		|| !FMath::IsFinite(BodyColor.R) || !FMath::IsFinite(BodyColor.G)
		|| !FMath::IsFinite(BodyColor.B) || !FMath::IsFinite(BodyColor.A))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, GetDefinitionId(),
			LOCTEXT("InvalidHeroTuning", "Hero movement, formation and presentation tuning contains invalid values."));
	}
	return FGolemResult::Success();
}

FGolemResult UGolemHeroDefinition::TrySetDefaultAbilitySetId(const FPrimaryAssetId& InAbilitySetId)
{
	if (!InAbilitySetId.IsValid() || InAbilitySetId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemAbilitySet")))
	{
		return FGolemResult::Failure(EGolemError::InvalidId, GetDefinitionId(),
			LOCTEXT("InvalidAbilitySetAssignment",
				"The proposed ability set must use the GolemAbilitySet primary asset type."));
	}
	DefaultAbilitySetId = InAbilitySetId;
	return FGolemResult::Success();
}

FGolemResult UGolemHeroDefinition::TrySetPresentation(const FLinearColor& InBodyColor, float InWalkSpeed,
	float InSprintSpeed, const FVector& InFormationOffset, float InFollowDistance)
{
	if (!FMath::IsFinite(InWalkSpeed) || !FMath::IsFinite(InSprintSpeed)
		|| !FMath::IsFinite(InFollowDistance) || InWalkSpeed <= 0.0f || InSprintSpeed < InWalkSpeed
		|| InFollowDistance <= 0.0f || InFormationOffset.ContainsNaN()
		|| !FMath::IsFinite(InBodyColor.R) || !FMath::IsFinite(InBodyColor.G)
		|| !FMath::IsFinite(InBodyColor.B) || !FMath::IsFinite(InBodyColor.A))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, GetDefinitionId(),
			LOCTEXT("InvalidPresentation", "The proposed hero tuning is invalid; the existing data was preserved."));
	}
	BodyColor = InBodyColor;
	WalkSpeed = InWalkSpeed;
	SprintSpeed = InSprintSpeed;
	FormationOffset = InFormationOffset;
	FollowDistance = InFollowDistance;
	return FGolemResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UGolemHeroDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FGolemResult Result = ValidateHero();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
