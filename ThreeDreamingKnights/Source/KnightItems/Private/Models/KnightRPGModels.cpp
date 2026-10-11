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

bool FKnightTalentLoadout::operator==(const FKnightTalentLoadout& Other) const
{
	return LoadoutId == Other.LoadoutId && Name == Other.Name && Build == Other.Build && Version == Other.Version;
}

bool FKnightRPGKnightView::IsInitialized() const
{
	return KnightId.IsValid() && KnightIdentity::IsValidDefinitionId(HeroDefinitionId)
		&& HeroDefinitionId.PrimaryAssetType == FPrimaryAssetType(TEXT("KnightHero")) && Revision >= 1
		&& SkillPoints >= 0 && SkillPoints <= KnightRPG::MaximumSkillPoints
		&& Level >= 1 && Level <= KnightGear::MaximumLevel
		&& (bGearEnabled ? ConsumableBindings.Num() == 2 : Level == 1 && ConsumableBindings.IsEmpty());
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
		&& StatBonuses == Other.StatBonuses && RuneTuning == Other.RuneTuning && Build == Other.Build
		&& WeaponRuneInstanceId == Other.WeaponRuneInstanceId && SkillRuneInstanceId == Other.SkillRuneInstanceId
		&& TalentLoadouts == Other.TalentLoadouts && ActiveTalentLoadoutId == Other.ActiveTalentLoadoutId
		&& bGearEnabled == Other.bGearEnabled && Level == Other.Level && ConsumableBindings == Other.ConsumableBindings;
}

bool FKnightRPGView::IsInitialized() const
{
	const bool bItemization = IsItemizationEnabled();
	return Revision >= 1 && InventoryId.IsValid() && InventoryRevision >= 1 && Knights.Num() == 3
		&& KnightIdentity::IsValidDefinitionId(SkillTreeId) && Currency >= 0 && Currency <= KnightGear::MaximumCurrency
		&& !Knights.ContainsByPredicate([bItemization](const FKnightRPGKnightView& Knight)
		{
			return Knight.bGearEnabled != bItemization;
		})
		&& (bGearDemo ? DemoRunId.IsValid() && bItemization
			: !DemoRunId.IsValid() && (bItemization || Currency == 0) && ClaimedRewards.IsEmpty());
}

bool FKnightRPGView::IsItemizationEnabled() const
{
	return Knights.Num() == 3 && !Knights.ContainsByPredicate([](const FKnightRPGKnightView& Knight)
	{
		return !Knight.bGearEnabled;
	});
}

bool FKnightRPGView::operator==(const FKnightRPGView& Other) const
{
	return Revision == Other.Revision && InventoryId == Other.InventoryId && InventoryRevision == Other.InventoryRevision
		&& SkillTreeId == Other.SkillTreeId && Knights == Other.Knights && Currency == Other.Currency
		&& DemoRunId == Other.DemoRunId && bGearDemo == Other.bGearDemo && ClaimedRewards == Other.ClaimedRewards;
}

bool FKnightRPGReceipt::operator==(const FKnightRPGReceipt& Other) const
{
	return HumanId == Other.HumanId && Command == Other.Command && InventoryRequest == Other.InventoryRequest
		&& PickedItem == Other.PickedItem && CommittedKnight == Other.CommittedKnight
		&& CommittedInventoryRevision == Other.CommittedInventoryRevision && AffectedInstanceId == Other.AffectedInstanceId
		&& CommittedKnights == Other.CommittedKnights && CommittedCurrency == Other.CommittedCurrency && Reward == Other.Reward;
}

bool FKnightRPGSnapshot::operator==(const FKnightRPGSnapshot& Other) const
{
	return SchemaVersion == Other.SchemaVersion && InitialSkillPoints == Other.InitialSkillPoints
		&& InitialView == Other.InitialView && InitialInventory == Other.InitialInventory && View == Other.View
		&& Inventory == Other.Inventory && Receipts == Other.Receipts && CombatStates.OrderIndependentCompareEqual(Other.CombatStates)
		&& RewardOffers == Other.RewardOffers
		&& LegacyWeaponRuneParents.OrderIndependentCompareEqual(Other.LegacyWeaponRuneParents)
		&& ItemizationMigration == Other.ItemizationMigration;
}

bool FKnightItemizationMigration::IsPresent() const
{
	return MigrationId.IsValid();
}

bool FKnightItemizationMigration::operator==(const FKnightItemizationMigration& Other) const
{
	return MigrationId == Other.MigrationId && LegacyReceiptCount == Other.LegacyReceiptCount
		&& InventoryRequest == Other.InventoryRequest;
}

bool FKnightRPGCombatSnapshot::operator==(const FKnightRPGCombatSnapshot& Other) const
{
	return Health == Other.Health && Stamina == Other.Stamina
		&& Skills.Resource == Other.Skills.Resource && Skills.CooldownRemaining == Other.Skills.CooldownRemaining
		&& Skills.Stacks == Other.Skills.Stacks && Skills.StackRemaining == Other.Skills.StackRemaining
		&& Skills.BuffRemaining == Other.Skills.BuffRemaining
		&& Skills.WaterReadyRemaining == Other.Skills.WaterReadyRemaining
		&& Skills.IceReadyRemaining == Other.Skills.IceReadyRemaining
		&& Skills.AftershockRemaining == Other.Skills.AftershockRemaining
		&& Skills.StackGainRemaining == Other.Skills.StackGainRemaining
		&& Skills.ResourceRegenDelayRemaining == Other.Skills.ResourceRegenDelayRemaining
		&& Skills.BulwarkRemaining == Other.Skills.BulwarkRemaining && Skills.LifeseedRemaining == Other.Skills.LifeseedRemaining
		&& Skills.StarfallRemaining == Other.Skills.StarfallRemaining && Skills.StarfallCount == Other.Skills.StarfallCount
		&& Skills.StarfallCounterRemaining == Other.Skills.StarfallCounterRemaining
		&& Skills.ConsumableRemaining == Other.Skills.ConsumableRemaining;
}

#undef LOCTEXT_NAMESPACE
