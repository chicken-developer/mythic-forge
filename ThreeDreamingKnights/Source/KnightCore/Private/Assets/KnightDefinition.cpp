#include "Assets/KnightDefinition.h"

#include "Types/KnightStableIds.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "KnightDefinition"

FPrimaryAssetId UKnightDefinition::GetPrimaryAssetId() const
{
	const FPrimaryAssetId Id(DefinitionType, DefinitionId);
	return KnightIdentity::IsValidDefinitionId(Id) ? Id : FPrimaryAssetId();
}

FName UKnightDefinition::GetDefinitionId() const
{
	return DefinitionId;
}

FName UKnightDefinition::GetDefinitionType() const
{
	return DefinitionType;
}

const FText& UKnightDefinition::GetDisplayName() const
{
	return DisplayName;
}

FKnightResult UKnightDefinition::ValidateIdentity() const
{
	if (!KnightIdentity::IsValidDefinitionId(FPrimaryAssetId(DefinitionType, DefinitionId)))
	{
		return FKnightResult::Failure(EKnightError::InvalidId, DefinitionId,
			LOCTEXT("InvalidIdentity", "A definition requires a stable ASCII type and ID (letters, digits, underscore)."));
	}
	return FKnightResult::Success();
}

bool UKnightDefinition::TrySetIdentity(FName InDefinitionId, FName InDefinitionType, FKnightResult* OutFailure)
{
	if (!KnightIdentity::IsValidDefinitionId(FPrimaryAssetId(InDefinitionType, InDefinitionId)))
	{
		if (OutFailure != nullptr)
		{
			*OutFailure = FKnightResult::Failure(EKnightError::InvalidId, InDefinitionId,
				LOCTEXT("InvalidAuthoringIdentity", "The proposed definition identity is invalid."));
		}
		return false;
	}

	DefinitionId = InDefinitionId;
	DefinitionType = InDefinitionType;
	if (OutFailure != nullptr)
	{
		*OutFailure = FKnightResult::Success();
	}
	return true;
}

void UKnightDefinition::SetDisplayName(const FText& InDisplayName)
{
	DisplayName = InDisplayName;
}

#if WITH_EDITOR
EDataValidationResult UKnightDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	const FKnightResult IdentityResult = ValidateIdentity();
	if (!IdentityResult.IsSuccess())
	{
		Context.AddError(IdentityResult.Message);
		return EDataValidationResult::Invalid;
	}
	return ParentResult == EDataValidationResult::Invalid ? ParentResult : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
