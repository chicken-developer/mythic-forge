#include "Definitions/KnightItemDefinition.h"

#include "Types/KnightStableIds.h"
#include "Misc/DataValidation.h"
#include "Tags/KnightGameplayTags.h"

#define LOCTEXT_NAMESPACE "KnightItemDefinition"

UKnightItemDefinition::UKnightItemDefinition()
{
	DefinitionType = TEXT("KnightItem");
}

void UKnightItemDefinition::PostLoad()
{
	Super::PostLoad();
	// Old serialized definitions predate metadata. Adapt only its untouched default value.
	if (Metadata.Category != EKnightItemCategory::Material || Metadata.Subtype != EKnightEquipmentSubtype::None
		|| Metadata.ItemLevel != 0 || !Metadata.AllowedSlots.IsEmpty() || !Metadata.Theme.IsNone())
	{
		return;
	}
	switch (ItemKind)
	{
	case EKnightItemKind::Equipment:
		Metadata.Category = EKnightItemCategory::Equipment;
		Metadata.Subtype = EKnightEquipmentSubtype::Hammer;
		Metadata.ItemLevel = 1;
		Metadata.AllowedSlots.Add(KnightGameplayTags::Slot_Equipment_Weapon);
		break;
	case EKnightItemKind::Rune: Metadata.Category = EKnightItemCategory::AttachRune; break;
	case EKnightItemKind::Consumable: Metadata.Category = EKnightItemCategory::Consumable; break;
	case EKnightItemKind::QuestItem: Metadata.Category = EKnightItemCategory::QuestItem; break;
	case EKnightItemKind::Junk: Metadata.Category = EKnightItemCategory::Junk; break;
	default: break;
	}
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

const FKnightItemMetadata& UKnightItemDefinition::GetMetadata() const
{
	return Metadata;
}

FKnightResult UKnightItemDefinition::TrySetMetadata(const FKnightItemMetadata& InMetadata)
{
	const FKnightResult Validation = InMetadata.Validate(GetPrimaryAssetId());
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	Metadata = InMetadata;
	return FKnightResult::Success();
}

FKnightItemStatBonuses UKnightItemDefinition::GetEffectiveStats(int32 UpgradeRank) const
{
	FKnightItemStatBonuses Result = StatBonuses;
	if (UpgradeRank == 1 && Metadata.UpgradeTrack == TEXT("RareOne"))
	{
		float* Values[] { &Result.MaxHealth, &Result.MaxStamina, &Result.MaxRuneEnergy,
			&Result.AttackPower, &Result.Defense, &Result.Poise };
		for (float* Value : Values)
		{
			if (*Value > 0.0f)
			{
				*Value = FMath::FloorToFloat(*Value * 1.10f + 0.5f);
			}
		}
	}
	return Result;
}

int32 UKnightItemDefinition::GetEffectiveItemLevel(int32 UpgradeRank) const
{
	return Metadata.ItemLevel + (UpgradeRank == 1 && Metadata.UpgradeTrack == TEXT("RareOne") ? 2 : 0);
}

FName UKnightItemDefinition::GetWeaponFamily() const
{
	return WeaponFamily;
}

FKnightResult UKnightItemDefinition::TrySetWeaponFamily(FName InFamily)
{
	if (!KnightIdentity::IsValidIdentifier(InFamily))
	{
		return FKnightResult::Failure(EKnightError::InvalidId, TEXT("WeaponFamily"),
			LOCTEXT("FamilyIdentity", "Weapon family needs a valid stable identifier."));
	}
	WeaponFamily = InFamily;
	return FKnightResult::Success();
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
	FKnightResult Result = ValidateTuning(ItemKind, MaximumStack, AbilitySetId, StatBonuses);
	if (Result.IsSuccess())
	{
		Result = Metadata.Validate(GetPrimaryAssetId());
	}
	const EKnightItemCategory ExpectedCategory = ItemKind == EKnightItemKind::Equipment
		? EKnightItemCategory::Equipment : ItemKind == EKnightItemKind::Rune ? EKnightItemCategory::AttachRune
		: ItemKind == EKnightItemKind::Consumable ? EKnightItemCategory::Consumable
		: ItemKind == EKnightItemKind::QuestItem ? EKnightItemCategory::QuestItem
		: ItemKind == EKnightItemKind::Junk ? EKnightItemCategory::Junk : EKnightItemCategory::Material;
	if (Result.IsSuccess() && Metadata.Category != ExpectedCategory)
	{
		return FKnightResult::Failure(EKnightError::TypeMismatch, TEXT("ItemCategory"),
			LOCTEXT("CategoryKind", "The authored metadata category must agree with the persistent item kind."));
	}
	return Result;
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
	Metadata = FKnightItemMetadata();
	switch (InKind)
	{
	case EKnightItemKind::Equipment:
		Metadata.Category = EKnightItemCategory::Equipment;
		Metadata.Subtype = EKnightEquipmentSubtype::Hammer;
		Metadata.ItemLevel = 1;
		Metadata.AllowedSlots.Add(KnightGameplayTags::Slot_Equipment_Weapon);
		break;
	case EKnightItemKind::Rune: Metadata.Category = EKnightItemCategory::AttachRune; break;
	case EKnightItemKind::Consumable: Metadata.Category = EKnightItemCategory::Consumable; break;
	case EKnightItemKind::QuestItem: Metadata.Category = EKnightItemCategory::QuestItem; break;
	case EKnightItemKind::Junk: Metadata.Category = EKnightItemCategory::Junk; break;
	default: Metadata.Category = EKnightItemCategory::Material; break;
	}
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
		&& InKind != EKnightItemKind::Equipment && InKind != EKnightItemKind::Rune
		&& InKind != EKnightItemKind::QuestItem && InKind != EKnightItemKind::Junk)
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
