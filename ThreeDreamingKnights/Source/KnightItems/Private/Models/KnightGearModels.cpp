#include "Models/KnightGearModels.h"

#include "Tags/KnightGameplayTags.h"

#define LOCTEXT_NAMESPACE "KnightGearModels"

const TArray<FGameplayTag>& KnightGear::EquipmentSlots()
{
	static const TArray<FGameplayTag> Slots
	{
		KnightGameplayTags::Slot_Equipment_Weapon, KnightGameplayTags::Slot_Equipment_Head,
		KnightGameplayTags::Slot_Equipment_Shoulders, KnightGameplayTags::Slot_Equipment_Chest,
		KnightGameplayTags::Slot_Equipment_Wrists, KnightGameplayTags::Slot_Equipment_Hands,
		KnightGameplayTags::Slot_Equipment_Waist, KnightGameplayTags::Slot_Equipment_Feet,
		KnightGameplayTags::Slot_Equipment_Neck, KnightGameplayTags::Slot_Equipment_Ring1,
		KnightGameplayTags::Slot_Equipment_Ring2, KnightGameplayTags::Slot_Equipment_Relic
	};
	return Slots;
}

FGameplayTag KnightGear::SlotForSubtype(EKnightEquipmentSubtype Subtype)
{
	const int32 Index = static_cast<int32>(Subtype) - 1;
	return Index >= 0 && Index < 11 ? EquipmentSlots()[Index == 10 ? 11 : Index] : FGameplayTag();
}

bool KnightGear::IsEquipmentSlot(FGameplayTag Slot)
{
	return EquipmentSlots().Contains(Slot);
}

FKnightResult FKnightItemMetadata::Validate(const FPrimaryAssetId& DefinitionId) const
{
	if (Category > EKnightItemCategory::Junk || Subtype > EKnightEquipmentSubtype::Relic
		|| Rarity > EKnightItemRarity::Epic || RequiredLevel < 1 || RequiredLevel > KnightGear::MaximumLevel
		|| ItemLevel < 0 || ItemLevel > 1000 || BuyPrice < 0 || SellPrice < 0 || SellPrice > 10000
		|| BuyPrice > 10000 || (BuyPrice > 0 && SellPrice > BuyPrice)
		|| UniqueEquipLimit < 0 || UniqueEquipLimit > 1)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("ItemMetadata"),
			LOCTEXT("Bounds", "Item taxonomy, requirement, tier, unique limit and prices need finite valid values."));
	}
	if (Category == EKnightItemCategory::Equipment)
	{
		if (Subtype == EKnightEquipmentSubtype::None || ItemLevel < 1 || AllowedSlots.IsEmpty()
			|| AllowedSlots.Num() > 2 || (Subtype == EKnightEquipmentSubtype::Ring
				&& (UniqueEquipGroup != DefinitionId || UniqueEquipLimit != 1 || AllowedSlots.Num() != 2)))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("EquipmentMetadata"),
				LOCTEXT("Equipment", "Equipment needs actual slots, a positive item level and definition-unique rings."));
		}
		for (FGameplayTag Slot : AllowedSlots)
		{
			if (!KnightGear::IsEquipmentSlot(Slot) || (Subtype == EKnightEquipmentSubtype::Ring
				? Slot != KnightGameplayTags::Slot_Equipment_Ring1 && Slot != KnightGameplayTags::Slot_Equipment_Ring2
				: Slot != KnightGear::SlotForSubtype(Subtype)))
			{
				return FKnightResult::Failure(EKnightError::TypeMismatch, TEXT("EquipmentSlot"),
					LOCTEXT("Slot", "Equipment slots must match the real equipment subtype."));
			}
		}
	}
	else if (Subtype != EKnightEquipmentSubtype::None || ItemLevel != 0 || !AllowedSlots.IsEmpty()
		|| UniqueEquipGroup.IsValid() || UniqueEquipLimit != 0 || !UpgradeTrack.IsNone() || !EquipEffectId.IsNone())
	{
		// Consumable effect IDs use the separate socket profile key Health/Resource.
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("NonEquipmentMetadata"),
			LOCTEXT("NonEquipment", "Non-equipment cannot carry equipment slots, item level, upgrades or equip effects."));
	}
	if ((!UpgradeTrack.IsNone() && (UpgradeTrack != TEXT("RareOne") || Rarity != EKnightItemRarity::Rare
		|| ItemLevel != 20)) || (!EquipEffectId.IsNone() && (Subtype != EKnightEquipmentSubtype::Relic
		|| (EquipEffectId != TEXT("Bulwark") && EquipEffectId != TEXT("Lifeseed") && EquipEffectId != TEXT("Starfall")))))
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("EquipEffect"),
			LOCTEXT("Effect", "Only real relic families and the authored Rare +1 upgrade track are supported."));
	}
	return FKnightResult::Success();
}

bool FKnightGearRewardItem::operator==(const FKnightGearRewardItem& Other) const
{
	return DefinitionId == Other.DefinitionId && Count == Other.Count;
}

bool FKnightGearRewardBundle::operator==(const FKnightGearRewardBundle& Other) const
{
	return RewardId == Other.RewardId && DemoRunId == Other.DemoRunId && EncounterId == Other.EncounterId
		&& Currency == Other.Currency && Level == Other.Level && Items == Other.Items;
}

#undef LOCTEXT_NAMESPACE
