#include "Models/GolemGearModels.h"

#include "Tags/GolemGameplayTags.h"

#define LOCTEXT_NAMESPACE "GolemGearModels"

const TArray<FGameplayTag>& GolemGear::EquipmentSlots()
{
	static const TArray<FGameplayTag> Slots
	{
		GolemGameplayTags::Slot_Equipment_Weapon, GolemGameplayTags::Slot_Equipment_Head,
		GolemGameplayTags::Slot_Equipment_Shoulders, GolemGameplayTags::Slot_Equipment_Chest,
		GolemGameplayTags::Slot_Equipment_Wrists, GolemGameplayTags::Slot_Equipment_Hands,
		GolemGameplayTags::Slot_Equipment_Waist, GolemGameplayTags::Slot_Equipment_Feet,
		GolemGameplayTags::Slot_Equipment_Neck, GolemGameplayTags::Slot_Equipment_Ring1,
		GolemGameplayTags::Slot_Equipment_Ring2, GolemGameplayTags::Slot_Equipment_Relic
	};
	return Slots;
}

FGameplayTag GolemGear::SlotForSubtype(EGolemEquipmentSubtype Subtype)
{
	const int32 Index = static_cast<int32>(Subtype) - 1;
	return Index >= 0 && Index < 11 ? EquipmentSlots()[Index == 10 ? 11 : Index] : FGameplayTag();
}

bool GolemGear::IsEquipmentSlot(FGameplayTag Slot)
{
	return EquipmentSlots().Contains(Slot);
}

FGolemResult FGolemItemMetadata::Validate(const FPrimaryAssetId& DefinitionId) const
{
	if (Category > EGolemItemCategory::Junk || Subtype > EGolemEquipmentSubtype::Relic
		|| Rarity > EGolemItemRarity::Epic || RequiredLevel < 1 || RequiredLevel > GolemGear::MaximumLevel
		|| ItemLevel < 0 || ItemLevel > 1000 || BuyPrice < 0 || SellPrice < 0 || SellPrice > 10000
		|| BuyPrice > 10000 || (BuyPrice > 0 && SellPrice > BuyPrice)
		|| UniqueEquipLimit < 0 || UniqueEquipLimit > 1)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("ItemMetadata"),
			LOCTEXT("Bounds", "Item taxonomy, requirement, tier, unique limit and prices need finite valid values."));
	}
	if (Category == EGolemItemCategory::Equipment)
	{
		if (Subtype == EGolemEquipmentSubtype::None || ItemLevel < 1 || AllowedSlots.IsEmpty()
			|| AllowedSlots.Num() > 2 || (Subtype == EGolemEquipmentSubtype::Ring
				&& (UniqueEquipGroup != DefinitionId || UniqueEquipLimit != 1 || AllowedSlots.Num() != 2)))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("EquipmentMetadata"),
				LOCTEXT("Equipment", "Equipment needs actual slots, a positive item level and definition-unique rings."));
		}
		for (FGameplayTag Slot : AllowedSlots)
		{
			if (!GolemGear::IsEquipmentSlot(Slot) || (Subtype == EGolemEquipmentSubtype::Ring
				? Slot != GolemGameplayTags::Slot_Equipment_Ring1 && Slot != GolemGameplayTags::Slot_Equipment_Ring2
				: Slot != GolemGear::SlotForSubtype(Subtype)))
			{
				return FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("EquipmentSlot"),
					LOCTEXT("Slot", "Equipment slots must match the real equipment subtype."));
			}
		}
	}
	else if (Subtype != EGolemEquipmentSubtype::None || ItemLevel != 0 || !AllowedSlots.IsEmpty()
		|| UniqueEquipGroup.IsValid() || UniqueEquipLimit != 0 || !UpgradeTrack.IsNone() || !EquipEffectId.IsNone())
	{
		// Consumable effect IDs use the separate socket profile key Health/Resource.
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("NonEquipmentMetadata"),
			LOCTEXT("NonEquipment", "Non-equipment cannot carry equipment slots, item level, upgrades or equip effects."));
	}
	if ((!UpgradeTrack.IsNone() && (UpgradeTrack != TEXT("RareOne") || Rarity != EGolemItemRarity::Rare
		|| ItemLevel != 20)) || (!EquipEffectId.IsNone() && (Subtype != EGolemEquipmentSubtype::Relic
		|| (EquipEffectId != TEXT("Bulwark") && EquipEffectId != TEXT("Lifeseed") && EquipEffectId != TEXT("Starfall")))))
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("EquipEffect"),
			LOCTEXT("Effect", "Only real relic families and the authored Rare +1 upgrade track are supported."));
	}
	return FGolemResult::Success();
}

bool FGolemGearRewardItem::operator==(const FGolemGearRewardItem& Other) const
{
	return DefinitionId == Other.DefinitionId && Count == Other.Count;
}

bool FGolemGearRewardBundle::operator==(const FGolemGearRewardBundle& Other) const
{
	return RewardId == Other.RewardId && DemoRunId == Other.DemoRunId && EncounterId == Other.EncounterId
		&& Currency == Other.Currency && Level == Other.Level && Items == Other.Items && BoundItems == Other.BoundItems;
}

#undef LOCTEXT_NAMESPACE
