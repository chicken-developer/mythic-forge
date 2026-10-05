#include "Definitions/KnightItemDefinition.h"

#include "Types/KnightStableIds.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "KnightItemDefinition"

UKnightItemDefinition::UKnightItemDefinition()
{
	DefinitionType = TEXT("KnightItem");
}

EKnightItemKind UKnightItemDefinition::GetItemKind() const
{
	return ItemKind;
}

int32 UKnightItemDefinition::GetMaximumStack() const
{
	return MaximumStack;
}

FPrimaryAssetId UKnightItemDefinition::GetAbilitySetId() const
{
	return AbilitySetId;
}

const FKnightItemStatBonuses& UKnightItemDefinition::GetStatBonuses() const
{
	return StatBonuses;
}

FKnightResult UKnightItemDefinition::ValidateItem() const
{
	const FKnightResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != FName(TEXT("KnightItem")))
	{
		return FKnightResult::Failure(EKnightError::TypeMismatch, GetDefinitionId(),
			LOCTEXT("InvalidItemType", "An item definition must use the KnightItem primary asset type."));
	}
	return ValidateTuning(ItemKind, MaximumStack, AbilitySetId, StatBonuses);
}

FKnightResult UKnightItemDefinition::TrySetItemTuning(EKnightItemKind InKind, int32 InMaximumStack,
	const FPrimaryAssetId& InAbilitySetId, const FKnightItemStatBonuses& InStatBonuses)
{
	const FKnightResult Result = ValidateTuning(InKind, InMaximumStack, InAbilitySetId, InStatBonuses);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	ItemKind = InKind;
	MaximumStack = InMaximumStack;
	AbilitySetId = InAbilitySetId;
	StatBonuses = InStatBonuses;
	return FKnightResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UKnightItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FKnightResult Result = ValidateItem();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif

FKnightResult UKnightItemDefinition::ValidateTuning(EKnightItemKind InKind, int32 InMaximumStack,
	const FPrimaryAssetId& InAbilitySetId, const FKnightItemStatBonuses& InStatBonuses)
{
	if (InKind != EKnightItemKind::Material && InKind != EKnightItemKind::Consumable
		&& InKind != EKnightItemKind::Equipment && InKind != EKnightItemKind::Rune)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("ItemTuning"),
			LOCTEXT("InvalidKind", "The item kind is not supported."));
	}
	if (InMaximumStack < 1 || InMaximumStack > KnightItems::MaximumStackCount)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("ItemTuning"),
			LOCTEXT("InvalidStack", "The maximum stack must be within the supported range."));
	}
	const FKnightResult Bonuses = InStatBonuses.Validate();
	if (!Bonuses.IsSuccess())
	{
		return Bonuses;
	}
	const bool bHasAbilitySet = InAbilitySetId != FPrimaryAssetId();
	if (bHasAbilitySet && (!KnightIdentity::IsValidDefinitionId(InAbilitySetId)
		|| InAbilitySetId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightAbilitySet"))))
	{
		return FKnightResult::Failure(EKnightError::InvalidId, TEXT("ItemTuning"),
			LOCTEXT("InvalidAbilitySet", "An optional item ability set requires a valid KnightAbilitySet identity."));
	}
	const bool bIsLoadoutItem = InKind == EKnightItemKind::Equipment || InKind == EKnightItemKind::Rune;
	if ((bIsLoadoutItem && InMaximumStack != 1)
		|| (!bIsLoadoutItem && (bHasAbilitySet || !InStatBonuses.IsEmpty())))
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("ItemTuning"),
			LOCTEXT("InvalidLoadoutTuning",
				"Equipment and runes are unstackable; ability grants and stat bonuses belong only to those item kinds."));
	}
	return FKnightResult::Success();
}

#undef LOCTEXT_NAMESPACE
