#include "Definitions/GolemItemDefinition.h"

#include "Types/GolemStableIds.h"
#include "Misc/DataValidation.h"
#include "Tags/GolemGameplayTags.h"

#define LOCTEXT_NAMESPACE "GolemItemDefinition"

UGolemItemDefinition::UGolemItemDefinition()
{
	DefinitionType = TEXT("GolemItem");
}

void UGolemItemDefinition::PostLoad()
{
	Super::PostLoad();
	// Old serialized definitions predate metadata. Adapt only its untouched default value.
	if (Metadata.Category != EGolemItemCategory::Material || Metadata.Subtype != EGolemEquipmentSubtype::None
		|| Metadata.ItemLevel != 0 || !Metadata.AllowedSlots.IsEmpty() || !Metadata.Theme.IsNone())
	{
		return;
	}
	switch (ItemKind)
	{
	case EGolemItemKind::Equipment:
		Metadata.Category = EGolemItemCategory::Equipment;
		Metadata.Subtype = EGolemEquipmentSubtype::Hammer;
		Metadata.ItemLevel = 1;
		Metadata.AllowedSlots.Add(GolemGameplayTags::Slot_Equipment_Weapon);
		break;
	case EGolemItemKind::Rune: Metadata.Category = EGolemItemCategory::AttachRune; break;
	case EGolemItemKind::Consumable: Metadata.Category = EGolemItemCategory::Consumable; break;
	case EGolemItemKind::QuestItem: Metadata.Category = EGolemItemCategory::QuestItem; break;
	case EGolemItemKind::Junk: Metadata.Category = EGolemItemCategory::Junk; break;
	default: break;
	}
}

EGolemItemKind UGolemItemDefinition::GetItemKind() const
{
	return ItemKind;
}

int32 UGolemItemDefinition::GetMaximumStack() const
{
	return MaximumStack;
}

FPrimaryAssetId UGolemItemDefinition::GetAbilitySetId() const
{
	return AbilitySetId;
}

const FGolemItemStatBonuses& UGolemItemDefinition::GetStatBonuses() const
{
	return StatBonuses;
}

const FGolemItemMetadata& UGolemItemDefinition::GetMetadata() const
{
	return Metadata;
}

FGolemResult UGolemItemDefinition::TrySetMetadata(const FGolemItemMetadata& InMetadata)
{
	const FGolemResult Validation = InMetadata.Validate(GetPrimaryAssetId());
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	Metadata = InMetadata;
	return FGolemResult::Success();
}

FGolemItemStatBonuses UGolemItemDefinition::GetEffectiveStats(int32 UpgradeRank) const
{
	FGolemItemStatBonuses Result = StatBonuses;
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

int32 UGolemItemDefinition::GetEffectiveItemLevel(int32 UpgradeRank) const
{
	return Metadata.ItemLevel + (UpgradeRank == 1 && Metadata.UpgradeTrack == TEXT("RareOne") ? 2 : 0);
}

FName UGolemItemDefinition::GetWeaponFamily() const
{
	return WeaponFamily;
}

FGolemResult UGolemItemDefinition::TrySetWeaponFamily(FName InFamily)
{
	if (!GolemIdentity::IsValidIdentifier(InFamily))
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("WeaponFamily"),
			LOCTEXT("FamilyIdentity", "Weapon family needs a valid stable identifier."));
	}
	WeaponFamily = InFamily;
	return FGolemResult::Success();
}

FGolemResult UGolemItemDefinition::ValidateItem() const
{
	const FGolemResult Identity = ValidateIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	if (GetDefinitionType() != FName(TEXT("GolemItem")))
	{
		return FGolemResult::Failure(EGolemError::TypeMismatch, GetDefinitionId(),
			LOCTEXT("InvalidItemType", "An item definition must use the GolemItem primary asset type."));
	}
	FGolemResult Result = ValidateTuning(ItemKind, MaximumStack, AbilitySetId, StatBonuses);
	if (Result.IsSuccess())
	{
		Result = Metadata.Validate(GetPrimaryAssetId());
	}
	const EGolemItemCategory ExpectedCategory = ItemKind == EGolemItemKind::Equipment
		? EGolemItemCategory::Equipment : ItemKind == EGolemItemKind::Rune ? EGolemItemCategory::AttachRune
		: ItemKind == EGolemItemKind::Consumable ? EGolemItemCategory::Consumable
		: ItemKind == EGolemItemKind::QuestItem ? EGolemItemCategory::QuestItem
		: ItemKind == EGolemItemKind::Junk ? EGolemItemCategory::Junk : EGolemItemCategory::Material;
	if (Result.IsSuccess() && Metadata.Category != ExpectedCategory)
	{
		return FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("ItemCategory"),
			LOCTEXT("CategoryKind", "The authored metadata category must agree with the persistent item kind."));
	}
	return Result;
}

FGolemResult UGolemItemDefinition::TrySetItemTuning(EGolemItemKind InKind, int32 InMaximumStack,
	const FPrimaryAssetId& InAbilitySetId, const FGolemItemStatBonuses& InStatBonuses)
{
	const FGolemResult Result = ValidateTuning(InKind, InMaximumStack, InAbilitySetId, InStatBonuses);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	ItemKind = InKind;
	MaximumStack = InMaximumStack;
	AbilitySetId = InAbilitySetId;
	StatBonuses = InStatBonuses;
	Metadata = FGolemItemMetadata();
	switch (InKind)
	{
	case EGolemItemKind::Equipment:
		Metadata.Category = EGolemItemCategory::Equipment;
		Metadata.Subtype = EGolemEquipmentSubtype::Hammer;
		Metadata.ItemLevel = 1;
		Metadata.AllowedSlots.Add(GolemGameplayTags::Slot_Equipment_Weapon);
		break;
	case EGolemItemKind::Rune: Metadata.Category = EGolemItemCategory::AttachRune; break;
	case EGolemItemKind::Consumable: Metadata.Category = EGolemItemCategory::Consumable; break;
	case EGolemItemKind::QuestItem: Metadata.Category = EGolemItemCategory::QuestItem; break;
	case EGolemItemKind::Junk: Metadata.Category = EGolemItemCategory::Junk; break;
	default: Metadata.Category = EGolemItemCategory::Material; break;
	}
	return FGolemResult::Success();
}

#if WITH_EDITOR
EDataValidationResult UGolemItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	const FGolemResult Result = ValidateItem();
	if (!Result.IsSuccess())
	{
		Context.AddError(Result.Message);
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif

FGolemResult UGolemItemDefinition::ValidateTuning(EGolemItemKind InKind, int32 InMaximumStack,
	const FPrimaryAssetId& InAbilitySetId, const FGolemItemStatBonuses& InStatBonuses)
{
	if (InKind != EGolemItemKind::Material && InKind != EGolemItemKind::Consumable
		&& InKind != EGolemItemKind::Equipment && InKind != EGolemItemKind::Rune
		&& InKind != EGolemItemKind::QuestItem && InKind != EGolemItemKind::Junk)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("ItemTuning"),
			LOCTEXT("InvalidKind", "The item kind is not supported."));
	}
	if (InMaximumStack < 1 || InMaximumStack > GolemItems::MaximumStackCount)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("ItemTuning"),
			LOCTEXT("InvalidStack", "The maximum stack must be within the supported range."));
	}
	const FGolemResult Bonuses = InStatBonuses.Validate();
	if (!Bonuses.IsSuccess())
	{
		return Bonuses;
	}
	const bool bHasAbilitySet = InAbilitySetId != FPrimaryAssetId();
	if (bHasAbilitySet && (!GolemIdentity::IsValidDefinitionId(InAbilitySetId)
		|| InAbilitySetId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemAbilitySet"))))
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("ItemTuning"),
			LOCTEXT("InvalidAbilitySet", "An optional item ability set requires a valid GolemAbilitySet identity."));
	}
	const bool bIsLoadoutItem = InKind == EGolemItemKind::Equipment || InKind == EGolemItemKind::Rune;
	if ((bIsLoadoutItem && InMaximumStack != 1)
		|| (!bIsLoadoutItem && (bHasAbilitySet || !InStatBonuses.IsEmpty())))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("ItemTuning"),
			LOCTEXT("InvalidLoadoutTuning",
				"Equipment and runes are unstackable; ability grants and stat bonuses belong only to those item kinds."));
	}
	return FGolemResult::Success();
}

#undef LOCTEXT_NAMESPACE
