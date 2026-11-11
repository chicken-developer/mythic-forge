#include "Models/GolemRPGModels.h"

#define LOCTEXT_NAMESPACE "GolemRPGModels"

FGolemResult FGolemRuneTuning::Validate() const
{
	const float Values[] { DamageScale, CostScale, DurationScale };
	for (float Value : Values)
	{
		if (!FMath::IsFinite(Value) || Value < 0.1f || Value > 5.0f)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RuneTuning"),
				LOCTEXT("RuneScales", "Rune multipliers must be finite values between 0.1 and 5."));
		}
	}
	return FGolemResult::Success();
}

bool FGolemRuneTuning::operator==(const FGolemRuneTuning& Other) const
{
	return DamageScale == Other.DamageScale && CostScale == Other.CostScale && DurationScale == Other.DurationScale;
}

bool FGolemMaterialCost::operator==(const FGolemMaterialCost& Other) const
{
	return DefinitionId == Other.DefinitionId && Count == Other.Count;
}

bool FGolemEquipmentAssignment::operator==(const FGolemEquipmentAssignment& Other) const
{
	return SlotTag == Other.SlotTag && ItemInstanceId == Other.ItemInstanceId && DefinitionId == Other.DefinitionId;
}

bool FGolemPurchasedSkill::operator==(const FGolemPurchasedSkill& Other) const
{
	return DefinitionId == Other.DefinitionId && PaidPoints == Other.PaidPoints && PaidMaterials == Other.PaidMaterials;
}

bool FGolemTalentLoadout::operator==(const FGolemTalentLoadout& Other) const
{
	return LoadoutId == Other.LoadoutId && Name == Other.Name && Build == Other.Build && Version == Other.Version;
}

bool FGolemRPGGolemView::IsInitialized() const
{
	return GolemId.IsValid() && GolemIdentity::IsValidDefinitionId(HeroDefinitionId)
		&& HeroDefinitionId.PrimaryAssetType == FPrimaryAssetType(TEXT("KnightHero")) && Revision >= 1
		&& SkillPoints >= 0 && SkillPoints <= GolemRPG::MaximumSkillPoints
		&& Level >= 1 && Level <= GolemGear::MaximumLevel
		&& (bGearEnabled ? ConsumableBindings.Num() == 2 : Level == 1 && ConsumableBindings.IsEmpty());
}

const FGolemEquipmentAssignment* FGolemRPGGolemView::FindAssignment(FGameplayTag SlotTag) const
{
	return Assignments.FindByPredicate([SlotTag](const FGolemEquipmentAssignment& Assignment)
		{
			return Assignment.SlotTag == SlotTag;
		});
}

bool FGolemRPGGolemView::HasSkill(const FPrimaryAssetId& SkillId) const
{
	return PurchasedSkills.ContainsByPredicate([&SkillId](const FGolemPurchasedSkill& Skill)
		{
			return Skill.DefinitionId == SkillId;
		});
}

bool FGolemRPGGolemView::operator==(const FGolemRPGGolemView& Other) const
{
	return GolemId == Other.GolemId && HeroDefinitionId == Other.HeroDefinitionId && Revision == Other.Revision
		&& SkillPoints == Other.SkillPoints && Assignments == Other.Assignments && PurchasedSkills == Other.PurchasedSkills
		&& StatBonuses == Other.StatBonuses && RuneTuning == Other.RuneTuning && Build == Other.Build
		&& WeaponRuneInstanceId == Other.WeaponRuneInstanceId && SkillRuneInstanceId == Other.SkillRuneInstanceId
		&& TalentLoadouts == Other.TalentLoadouts && ActiveTalentLoadoutId == Other.ActiveTalentLoadoutId
		&& bGearEnabled == Other.bGearEnabled && Level == Other.Level && ConsumableBindings == Other.ConsumableBindings;
}

bool FGolemRPGView::IsInitialized() const
{
	const bool bItemization = IsItemizationEnabled();
	return Revision >= 1 && InventoryId.IsValid() && InventoryRevision >= 1 && Golems.Num() == 3
		&& GolemIdentity::IsValidDefinitionId(SkillTreeId) && Currency >= 0 && Currency <= GolemGear::MaximumCurrency
		&& !Golems.ContainsByPredicate([bItemization](const FGolemRPGGolemView& Golem)
		{
			return Golem.bGearEnabled != bItemization;
		})
		&& (bGearDemo ? DemoRunId.IsValid() && bItemization
			: !DemoRunId.IsValid() && (bItemization || Currency == 0) && ClaimedRewards.IsEmpty());
}

bool FGolemRPGView::IsItemizationEnabled() const
{
	return Golems.Num() == 3 && !Golems.ContainsByPredicate([](const FGolemRPGGolemView& Golem)
	{
		return !Golem.bGearEnabled;
	});
}

bool FGolemRPGView::operator==(const FGolemRPGView& Other) const
{
	return Revision == Other.Revision && InventoryId == Other.InventoryId && InventoryRevision == Other.InventoryRevision
		&& SkillTreeId == Other.SkillTreeId && Golems == Other.Golems && Currency == Other.Currency
		&& DemoRunId == Other.DemoRunId && bGearDemo == Other.bGearDemo && ClaimedRewards == Other.ClaimedRewards;
}

bool FGolemRPGReceipt::operator==(const FGolemRPGReceipt& Other) const
{
	return HumanId == Other.HumanId && Command == Other.Command && InventoryRequest == Other.InventoryRequest
		&& PickedItem == Other.PickedItem && CommittedGolem == Other.CommittedGolem
		&& CommittedInventoryRevision == Other.CommittedInventoryRevision && AffectedInstanceId == Other.AffectedInstanceId
		&& CommittedGolems == Other.CommittedGolems && CommittedCurrency == Other.CommittedCurrency && Reward == Other.Reward;
}

bool FGolemRPGSnapshot::operator==(const FGolemRPGSnapshot& Other) const
{
	return SchemaVersion == Other.SchemaVersion && InitialSkillPoints == Other.InitialSkillPoints
		&& InitialView == Other.InitialView && InitialInventory == Other.InitialInventory && View == Other.View
		&& Inventory == Other.Inventory && Receipts == Other.Receipts && CombatStates.OrderIndependentCompareEqual(Other.CombatStates)
		&& RewardOffers == Other.RewardOffers
		&& LegacyWeaponRuneParents.OrderIndependentCompareEqual(Other.LegacyWeaponRuneParents)
		&& ItemizationMigration == Other.ItemizationMigration;
}

bool FGolemItemizationMigration::IsPresent() const
{
	return MigrationId.IsValid();
}

bool FGolemItemizationMigration::operator==(const FGolemItemizationMigration& Other) const
{
	return MigrationId == Other.MigrationId && LegacyReceiptCount == Other.LegacyReceiptCount
		&& InventoryRequest == Other.InventoryRequest;
}

bool FGolemRPGCombatSnapshot::operator==(const FGolemRPGCombatSnapshot& Other) const
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
