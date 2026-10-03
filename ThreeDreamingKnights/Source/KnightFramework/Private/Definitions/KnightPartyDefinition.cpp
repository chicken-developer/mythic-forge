#include "Definitions/KnightPartyDefinition.h"

#include "Settings/KnightSettings.h"
#include "Types/KnightStableIds.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "KnightPartyDefinition"

UKnightPartyDefinition::UKnightPartyDefinition()
{
	DefinitionType = TEXT("KnightParty");
}

const TArray<FPrimaryAssetId>& UKnightPartyDefinition::GetHeroDefinitions() const
{
	return HeroDefinitions;
}

FKnightResult UKnightPartyDefinition::ValidateParty() const
{
	const FKnightResult IdentityResult = ValidateIdentity();
	if (!IdentityResult.IsSuccess())
	{
		return IdentityResult;
	}
	if (GetDefinitionType() != FName(TEXT("KnightParty")))
	{
		return FKnightResult::Failure(EKnightError::TypeMismatch, GetDefinitionId(),
			LOCTEXT("WrongPartyType", "A party definition must use the KnightParty primary asset type."));
	}
	return ValidateHeroIds(HeroDefinitions);
}

FKnightResult UKnightPartyDefinition::TrySetHeroDefinitions(const TArray<FPrimaryAssetId>& InHeroDefinitions)
{
	const FKnightResult Result = ValidateHeroIds(InHeroDefinitions);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	HeroDefinitions = InHeroDefinitions;
	return FKnightResult::Success();
}

FKnightResult UKnightPartyDefinition::ValidateHeroIds(const TArray<FPrimaryAssetId>& HeroIds)
{
	if (HeroIds.Num() != UKnightSettings::CanonicalPartySize)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("PartyDefinition"),
			LOCTEXT("WrongPartyCount", "The canonical party requires exactly three hero definitions."));
	}
	TSet<FPrimaryAssetId> UniqueIds;
	for (const FPrimaryAssetId& HeroId : HeroIds)
	{
		if (!KnightIdentity::IsValidDefinitionId(HeroId)
			|| HeroId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightHero")))
		{
			return FKnightResult::Failure(EKnightError::InvalidId, FName(*HeroId.ToString()),
				LOCTEXT("InvalidHeroId", "Every party entry requires a valid KnightHero primary asset ID."));
		}
		if (UniqueIds.Contains(HeroId))
		{
			return FKnightResult::Failure(EKnightError::DuplicateId, FName(*HeroId.ToString()),
				LOCTEXT("DuplicateHeroId", "The three canonical Knights require distinct hero definitions."));
		}
		UniqueIds.Add(HeroId);
	}
	return FKnightResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UKnightPartyDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FKnightResult Result = ValidateParty();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif

#undef LOCTEXT_NAMESPACE
