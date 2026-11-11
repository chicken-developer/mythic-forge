#include "Definitions/GolemPartyDefinition.h"

#include "Settings/GolemSettings.h"
#include "Types/GolemStableIds.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "KnightPartyDefinition"

UGolemPartyDefinition::UGolemPartyDefinition()
{
	DefinitionType = TEXT("KnightParty");
}

const TArray<FPrimaryAssetId>& UGolemPartyDefinition::GetHeroDefinitions() const
{
	return HeroDefinitions;
}

FGolemResult UGolemPartyDefinition::ValidateParty() const
{
	const FGolemResult IdentityResult = ValidateIdentity();
	if (!IdentityResult.IsSuccess())
	{
		return IdentityResult;
	}
	if (GetDefinitionType() != FName(TEXT("KnightParty")))
	{
		return FGolemResult::Failure(EGolemError::TypeMismatch, GetDefinitionId(),
			LOCTEXT("WrongPartyType", "A party definition must use the GolemParty primary asset type."));
	}
	return ValidateHeroIds(HeroDefinitions);
}

FGolemResult UGolemPartyDefinition::TrySetHeroDefinitions(const TArray<FPrimaryAssetId>& InHeroDefinitions)
{
	const FGolemResult Result = ValidateHeroIds(InHeroDefinitions);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	HeroDefinitions = InHeroDefinitions;
	return FGolemResult::Success();
}

FGolemResult UGolemPartyDefinition::ValidateHeroIds(const TArray<FPrimaryAssetId>& HeroIds)
{
	if (HeroIds.Num() != UGolemSettings::CanonicalPartySize)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("PartyDefinition"),
			LOCTEXT("WrongPartyCount", "The canonical party requires exactly three hero definitions."));
	}
	TSet<FPrimaryAssetId> UniqueIds;
	for (const FPrimaryAssetId& HeroId : HeroIds)
	{
		if (!GolemIdentity::IsValidDefinitionId(HeroId)
			|| HeroId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightHero")))
		{
			return FGolemResult::Failure(EGolemError::InvalidId, FName(*HeroId.ToString()),
				LOCTEXT("InvalidHeroId", "Every party entry requires a valid GolemHero primary asset ID."));
		}
		if (UniqueIds.Contains(HeroId))
		{
			return FGolemResult::Failure(EGolemError::DuplicateId, FName(*HeroId.ToString()),
				LOCTEXT("DuplicateHeroId", "The three canonical Golems require distinct hero definitions."));
		}
		UniqueIds.Add(HeroId);
	}
	return FGolemResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UGolemPartyDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FGolemResult Result = ValidateParty();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
