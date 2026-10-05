#include "Models/KnightRPGModels.h"

#define LOCTEXT_NAMESPACE "KnightRPGModels"

FKnightResult FKnightRuneTuning::Validate() const
{
	const float Values[] { DamageScale, CostScale, DurationScale };
	for (float Value : Values)
	{
		if (!FMath::IsFinite(Value) || Value < 0.1f || Value > 5.0f)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RuneTuning"),
				LOCTEXT("RuneScales", "Rune multipliers must be finite values between 0.1 and 5."));
		}
	}
	return FKnightResult::Success();
}

bool FKnightRuneTuning::operator==(const FKnightRuneTuning& Other) const
{
	return DamageScale == Other.DamageScale && CostScale == Other.CostScale && DurationScale == Other.DurationScale;
}

bool FKnightMaterialCost::operator==(const FKnightMaterialCost& Other) const
{
	return DefinitionId == Other.DefinitionId && Count == Other.Count;
}

bool FKnightEquipmentAssignment::operator==(const FKnightEquipmentAssignment& Other) const
{
	return SlotTag == Other.SlotTag && ItemInstanceId == Other.ItemInstanceId && DefinitionId == Other.DefinitionId;
}

bool FKnightPurchasedSkill::operator==(const FKnightPurchasedSkill& Other) const
{
	return DefinitionId == Other.DefinitionId && PaidPoints == Other.PaidPoints && PaidMaterials == Other.PaidMaterials;
}

bool FKnightRPGKnightView::IsInitialized() const
{
	return KnightId.IsValid() && KnightIdentity::IsValidDefinitionId(HeroDefinitionId)
		&& HeroDefinitionId.PrimaryAssetType == FPrimaryAssetType(TEXT("KnightHero")) && Revision >= 1
		&& SkillPoints >= 0 && SkillPoints <= KnightRPG::MaximumSkillPoints;
}

const FKnightEquipmentAssignment* FKnightRPGKnightView::FindAssignment(FGameplayTag SlotTag) const
{
	return Assignments.FindByPredicate([SlotTag](const FKnightEquipmentAssignment& Assignment)
		{
			return Assignment.SlotTag == SlotTag;
		});
}

bool FKnightRPGKnightView::HasSkill(const FPrimaryAssetId& SkillId) const
{
	return PurchasedSkills.ContainsByPredicate([&SkillId](const FKnightPurchasedSkill& Skill)
		{
			return Skill.DefinitionId == SkillId;
		});
}

bool FKnightRPGKnightView::operator==(const FKnightRPGKnightView& Other) const
{
	return KnightId == Other.KnightId && HeroDefinitionId == Other.HeroDefinitionId && Revision == Other.Revision
		&& SkillPoints == Other.SkillPoints && Assignments == Other.Assignments && PurchasedSkills == Other.PurchasedSkills
		&& StatBonuses == Other.StatBonuses && RuneTuning == Other.RuneTuning;
}

bool FKnightRPGView::IsInitialized() const
{
	return Revision >= 1 && InventoryId.IsValid() && InventoryRevision >= 1 && Knights.Num() == 3
		&& KnightIdentity::IsValidDefinitionId(SkillTreeId);
}

bool FKnightRPGView::operator==(const FKnightRPGView& Other) const
{
	return Revision == Other.Revision && InventoryId == Other.InventoryId && InventoryRevision == Other.InventoryRevision
		&& SkillTreeId == Other.SkillTreeId && Knights == Other.Knights;
}

bool FKnightRPGReceipt::operator==(const FKnightRPGReceipt& Other) const
{
	return HumanId == Other.HumanId && Command == Other.Command && InventoryRequest == Other.InventoryRequest
		&& PickedItem == Other.PickedItem && CommittedKnight == Other.CommittedKnight
		&& CommittedInventoryRevision == Other.CommittedInventoryRevision && AffectedInstanceId == Other.AffectedInstanceId;
}

bool FKnightRPGSnapshot::operator==(const FKnightRPGSnapshot& Other) const
{
	return SchemaVersion == Other.SchemaVersion && InitialSkillPoints == Other.InitialSkillPoints
		&& InitialView == Other.InitialView && InitialInventory == Other.InitialInventory && View == Other.View
		&& Inventory == Other.Inventory && Receipts == Other.Receipts;
}

#undef LOCTEXT_NAMESPACE
