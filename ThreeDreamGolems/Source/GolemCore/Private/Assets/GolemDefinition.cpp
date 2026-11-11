#include "Assets/GolemDefinition.h"

#include "Types/GolemStableIds.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "GolemDefinition"

FPrimaryAssetId UGolemDefinition::GetPrimaryAssetId() const
{
	const FPrimaryAssetId Id(DefinitionType, DefinitionId);
	return GolemIdentity::IsValidDefinitionId(Id) ? Id : FPrimaryAssetId();
}

FName UGolemDefinition::GetDefinitionId() const
{
	return DefinitionId;
}

FName UGolemDefinition::GetDefinitionType() const
{
	return DefinitionType;
}

const FText& UGolemDefinition::GetDisplayName() const
{
	return DisplayName;
}

FGolemResult UGolemDefinition::ValidateIdentity() const
{
	if (!GolemIdentity::IsValidDefinitionId(FPrimaryAssetId(DefinitionType, DefinitionId)))
	{
		return FGolemResult::Failure(EGolemError::InvalidId, DefinitionId,
			LOCTEXT("InvalidIdentity", "A definition requires a stable ASCII type and ID (letters, digits, underscore)."));
	}
	return FGolemResult::Success();
}

bool UGolemDefinition::TrySetIdentity(FName InDefinitionId, FName InDefinitionType, FGolemResult* OutFailure)
{
	if (!GolemIdentity::IsValidDefinitionId(FPrimaryAssetId(InDefinitionType, InDefinitionId)))
	{
		if (OutFailure != nullptr)
		{
			*OutFailure = FGolemResult::Failure(EGolemError::InvalidId, InDefinitionId,
				LOCTEXT("InvalidAuthoringIdentity", "The proposed definition identity is invalid."));
		}
		return false;
	}

	DefinitionId = InDefinitionId;
	DefinitionType = InDefinitionType;
	if (OutFailure != nullptr)
	{
		*OutFailure = FGolemResult::Success();
	}
	return true;
}

void UGolemDefinition::SetDisplayName(const FText& InDisplayName)
{
	DisplayName = InDisplayName;
}

#if WITH_EDITOR
EDataValidationResult UGolemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	const FGolemResult IdentityResult = ValidateIdentity();
	if (!IdentityResult.IsSuccess())
	{
		Context.AddError(IdentityResult.Message);
		return EDataValidationResult::Invalid;
	}
	return ParentResult == EDataValidationResult::Invalid ? ParentResult : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
