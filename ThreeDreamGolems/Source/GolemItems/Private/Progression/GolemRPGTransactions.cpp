#include "Progression/GolemRPGComponent.h"

#include "Assets/GolemDefinition.h"
#include "Definitions/GolemItemDefinition.h"
#include "Definitions/GolemRuneDefinition.h"
#include "Definitions/GolemSkillDefinition.h"
#include "Tags/GolemGameplayTags.h"

#define LOCTEXT_NAMESPACE "GolemRPGTransactions"

FGolemResult UGolemRPGComponent::BuildCandidate(const FGolemRPGView& Existing,
	const FGolemInventoryView& InventoryView, const FGolemRPGCommand& Command,
	const FGolemItemInstance* PickedItem, const TArray<FGolemInventoryMutation>* HistoricalMutations,
	FGolemRPGView& OutView, FGolemInventoryRequest& OutInventoryRequest, FGuid& OutAffectedInstance) const
{
	OutView = Existing;
	OutInventoryRequest = FGolemInventoryRequest();
	OutInventoryRequest.RequestId = Command.RequestId;
	OutInventoryRequest.ExpectedRevision = Command.ExpectedInventoryRevision;
	OutAffectedInstance.Invalidate();
	FGolemRPGGolemView* Golem = OutView.Golems.FindByPredicate([&Command](const FGolemRPGGolemView& Entry)
		{
			return Entry.GolemId == Command.ExpectedGolemId;
		});
	if (Golem == nullptr || !Golem->IsInitialized() || Command.InventoryId != InventoryView.InventoryId
		|| Command.ExpectedInventoryRevision != InventoryView.Revision || Command.ExpectedGolemRevision != Golem->Revision
		|| Existing.Revision >= MAX_int64 || Golem->Revision >= MAX_int64)
	{
		return FGolemResult::Failure(EGolemError::InvalidRevision, TEXT("RPGCandidate"),
			LOCTEXT("CandidateRevision", "A candidate must start from exact current inventory and Golem revisions."));
	}
	TSet<FGuid> AssignedIds;
	for (const FGolemRPGGolemView& ExistingGolem : Existing.Golems)
	{
		const FGuid RuneIds[] { ExistingGolem.WeaponRuneInstanceId, ExistingGolem.SkillRuneInstanceId };
		for (const FGuid& RuneId : RuneIds)
		{
			if (RuneId.IsValid())
			{
				if (AssignedIds.Contains(RuneId))
				{
					return FGolemResult::Failure(EGolemError::DuplicateId, TEXT("RPGAttachments"),
						LOCTEXT("AttachmentDuplicate", "A rune instance has exactly one attachment owner."));
				}
				AssignedIds.Add(RuneId);
			}
		}
		for (const FGolemEquipmentAssignment& Assignment : ExistingGolem.Assignments)
		{
			if (!Assignment.ItemInstanceId.IsValid() || AssignedIds.Contains(Assignment.ItemInstanceId))
			{
				return FGolemResult::Failure(EGolemError::DuplicateId, TEXT("RPGAssignments"),
					LOCTEXT("AssignmentDuplicate", "An item instance may be assigned to only one canonical Golem and slot."));
			}
			AssignedIds.Add(Assignment.ItemInstanceId);
		}
	}
	if (Existing.IsItemizationEnabled() && (Command.Operation == EGolemRPGOperation::Equip
		|| Command.Operation == EGolemRPGOperation::Unequip
		|| Command.Operation >= EGolemRPGOperation::TransferEquipment))
	{
		const FGolemResult Result = BuildGearCandidate(OutView, InventoryView, Command,
			HistoricalMutations, OutInventoryRequest, OutAffectedInstance);
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	else if (Command.Operation == EGolemRPGOperation::Pickup)
	{
		if (PickedItem == nullptr || !PickedItem->ValidateIdentityAndCount().IsSuccess())
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RPGPickupCandidate"),
				LOCTEXT("PickedItem", "A pickup requires the exact validated item from its reserved authoritative actor."));
		}
		FGolemInventoryMutation Mutation;
		Mutation.Item = *PickedItem;
		OutInventoryRequest.Mutations.Add(Mutation);
		OutAffectedInstance = PickedItem->InstanceId;
	}
	else if (Command.Operation == EGolemRPGOperation::Equip || Command.Operation == EGolemRPGOperation::Rune)
	{
		const FGolemItemInstance* Item = InventoryView.Items.FindByPredicate([&Command](const FGolemItemInstance& Entry)
			{
				return Entry.InstanceId == Command.ItemInstanceId;
			});
		if (Item == nullptr)
		{
			return FGolemResult::Failure(EGolemError::NotFound, TEXT("RPGEquip"),
				LOCTEXT("EquipInstance", "The exact equipment instance is not present in shared inventory."));
		}
		const UGolemDefinition* Resolved = nullptr;
		const FGolemResult Resolution = ResolveDefinition(Item->DefinitionId, UGolemItemDefinition::StaticClass(), Resolved);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		const UGolemItemDefinition* Definition = CastChecked<UGolemItemDefinition>(Resolved);
		const FGolemResult Validation = Definition->ValidateItem();
		if (!Validation.IsSuccess())
		{
			return Validation;
		}
		const bool bRune = Command.Operation == EGolemRPGOperation::Rune;
		const UGolemRuneDefinition* AttachmentRune = Cast<UGolemRuneDefinition>(Definition);
		if (bRune && AttachmentRune != nullptr && !AttachmentRune->GetAttachmentRuneId().IsNone())
		{
			return FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("RuneSocket"),
				LOCTEXT("AttachmentSocket", "Attachment runes use the explicit weapon or skill socket in ApplyBuild."));
		}
		if (Item->Count != 1 || (bRune && (Definition->GetItemKind() != EGolemItemKind::Rune
			|| !Definition->IsA<UGolemRuneDefinition>() || Command.SlotTag != GolemGameplayTags::Slot_Equipment_Rune))
			|| (!bRune && (Definition->GetItemKind() != EGolemItemKind::Equipment
				|| Command.SlotTag != GolemGameplayTags::Slot_Equipment_Weapon)))
		{
			return FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("RPGEquipSlot"),
				LOCTEXT("SlotKind", "The selected item kind must match its native weapon or rune slot."));
		}
		if (AssignedIds.Contains(Item->InstanceId))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGEquipAssignment"),
				LOCTEXT("AlreadyAssigned", "The equipment instance is already assigned; unequip its current owner first."));
		}
		Golem->Assignments.RemoveAll([&Command](const FGolemEquipmentAssignment& Assignment)
			{
				return Assignment.SlotTag == Command.SlotTag;
			});
		FGolemEquipmentAssignment Assignment;
		Assignment.SlotTag = Command.SlotTag;
		Assignment.ItemInstanceId = Item->InstanceId;
		Assignment.DefinitionId = Item->DefinitionId;
		Golem->Assignments.Add(Assignment);
		OutAffectedInstance = Item->InstanceId;
	}
	else if (Command.Operation == EGolemRPGOperation::Unequip)
	{
		const FGolemEquipmentAssignment* Assignment = Golem->FindAssignment(Command.SlotTag);
		if (Assignment == nullptr || Assignment->ItemInstanceId != Command.ItemInstanceId)
		{
			return FGolemResult::Failure(EGolemError::NotOwned, TEXT("RPGUnequip"),
				LOCTEXT("UnequipIdentity", "Unequip requires this Golem's exact currently assigned slot and instance."));
		}
		OutAffectedInstance = Assignment->ItemInstanceId;
		Golem->Assignments.RemoveAll([&Command](const FGolemEquipmentAssignment& Entry)
			{
				return Entry.SlotTag == Command.SlotTag;
			});
	}
	else if (Command.Operation == EGolemRPGOperation::PurchaseSkill)
	{
		const TObjectPtr<UGolemSkillDefinition>* SkillPointer = Skills.Find(Command.SkillDefinitionId);
		const UGolemSkillDefinition* Skill = SkillPointer == nullptr ? nullptr : SkillPointer->Get();
		if (!IsValid(Skill))
		{
			return FGolemResult::Failure(EGolemError::NotFound, TEXT("RPGPurchaseSkill"),
				LOCTEXT("SkillNotInTree", "The requested actual skill is not a validated node in this campaign's skill tree."));
		}
		if (Golem->HasSkill(Command.SkillDefinitionId))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGPurchaseSkill"),
				LOCTEXT("SkillOwned", "This Golem already purchased the exact skill."));
		}
		for (const FPrimaryAssetId& Required : Skill->GetPrerequisites())
		{
			if (!Golem->HasSkill(Required))
			{
				return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGPurchaseSkill"),
					LOCTEXT("Prerequisite", "Purchase the skill's actual prerequisites first."));
			}
		}
		if (Golem->SkillPoints < Skill->GetPointCost())
		{
			return FGolemResult::Failure(EGolemError::InsufficientResources, TEXT("RPGSkillPoints"),
				LOCTEXT("Points", "This Golem does not have enough skill points."));
		}
		FGolemPurchasedSkill Purchase;
		Purchase.DefinitionId = Command.SkillDefinitionId;
		Purchase.PaidPoints = Skill->GetPointCost();
		TArray<FGolemItemInstance> SortedItems = InventoryView.Items;
		SortedItems.Sort([](const FGolemItemInstance& Left, const FGolemItemInstance& Right)
			{
				return Left.InstanceId < Right.InstanceId;
			});
		for (const FGolemMaterialCost& Cost : Skill->GetMaterialCosts())
		{
			int32 Remaining = Cost.Count;
			for (const FGolemItemInstance& Item : SortedItems)
			{
				if (Remaining == 0)
				{
					break;
				}
				if (Item.DefinitionId != Cost.DefinitionId || AssignedIds.Contains(Item.InstanceId))
				{
					continue;
				}
				FGolemInventoryMutation Mutation;
				Mutation.Operation = EGolemInventoryOperation::Remove;
				Mutation.Item = Item;
				Mutation.Item.Count = FMath::Min(Remaining, Item.Count);
				Remaining -= Mutation.Item.Count;
				OutInventoryRequest.Mutations.Add(Mutation);
				Purchase.PaidMaterials.Add(Mutation.Item);
			}
			if (Remaining > 0)
			{
				return FGolemResult::Failure(EGolemError::InsufficientResources, TEXT("RPGSkillMaterials"),
					LOCTEXT("Materials", "Shared inventory does not contain the complete unassigned material cost."));
			}
		}
		Golem->SkillPoints -= Purchase.PaidPoints;
		Golem->PurchasedSkills.Add(MoveTemp(Purchase));
	}
	else if (Command.Operation == EGolemRPGOperation::Respec)
	{
		if (Golem->PurchasedSkills.IsEmpty())
		{
			return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGRespec"),
				LOCTEXT("NoPaidSkills", "This Golem has no paid skills to refund."));
		}
		TMap<FPrimaryAssetId, int64> Refunds;
		int64 RefundedPoints = Golem->SkillPoints;
		for (const FGolemPurchasedSkill& Purchase : Golem->PurchasedSkills)
		{
			RefundedPoints += Purchase.PaidPoints;
			for (const FGolemItemInstance& Paid : Purchase.PaidMaterials)
			{
				Refunds.FindOrAdd(Paid.DefinitionId) += Paid.Count;
			}
		}
		if (RefundedPoints > GolemRPG::MaximumSkillPoints)
		{
			return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("RPGRespec"),
				LOCTEXT("RefundPointBound", "The retained paid point refund exceeds the supported finite balance."));
		}
		TArray<FPrimaryAssetId> Definitions;
		Refunds.GetKeys(Definitions);
		Definitions.Sort([](const FPrimaryAssetId& Left, const FPrimaryAssetId& Right)
			{
				return Left.ToString() < Right.ToString();
			});
		TArray<FGolemItemInstance> SortedItems = InventoryView.Items;
		SortedItems.Sort([](const FGolemItemInstance& Left, const FGolemItemInstance& Right)
			{
				return Left.InstanceId < Right.InstanceId;
			});
		for (const FPrimaryAssetId& Id : Definitions)
		{
			const UGolemDefinition* Resolved = nullptr;
			const FGolemResult Resolution = ResolveDefinition(Id, UGolemItemDefinition::StaticClass(), Resolved);
			if (!Resolution.IsSuccess())
			{
				return Resolution;
			}
			const UGolemItemDefinition* Definition = CastChecked<UGolemItemDefinition>(Resolved);
			if (Definition->GetItemKind() != EGolemItemKind::Material || !Definition->ValidateItem().IsSuccess())
			{
				return FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("RPGRefund"),
					LOCTEXT("RefundMaterial", "Retained material refunds require actual validated Material definitions."));
			}
			int64 Remaining = Refunds.FindChecked(Id);
			for (const FGolemItemInstance& Item : SortedItems)
			{
				if (Remaining == 0)
				{
					break;
				}
				if (Item.DefinitionId != Id || AssignedIds.Contains(Item.InstanceId))
				{
					continue;
				}
				const int32 Room = Definition->GetMaximumStack() - Item.Count;
				if (Room <= 0)
				{
					continue;
				}
				FGolemInventoryMutation Mutation;
				Mutation.Item = Item;
				Mutation.Item.Count = static_cast<int32>(FMath::Min<int64>(Remaining, Room));
				Remaining -= Mutation.Item.Count;
				OutInventoryRequest.Mutations.Add(Mutation);
			}
			while (Remaining > 0)
			{
				FGolemInventoryMutation Mutation;
				Mutation.Item.InstanceId = FGuid::NewGuid();
				Mutation.Item.DefinitionId = Id;
				Mutation.Item.Count = static_cast<int32>(FMath::Min<int64>(Remaining, Definition->GetMaximumStack()));
				const int32 MutationIndex = OutInventoryRequest.Mutations.Num();
				if (HistoricalMutations != nullptr && HistoricalMutations->IsValidIndex(MutationIndex))
				{
					Mutation.Item.InstanceId = (*HistoricalMutations)[MutationIndex].Item.InstanceId;
				}
				Remaining -= Mutation.Item.Count;
				OutInventoryRequest.Mutations.Add(Mutation);
				if (OutInventoryRequest.Mutations.Num() > GolemItems::MaximumBatchOperations)
				{
					return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("RPGRefund"),
						LOCTEXT("RefundBatch", "The retained refund exceeds the supported finite atomic mutation batch."));
				}
			}
		}
		Golem->SkillPoints = static_cast<int32>(RefundedPoints);
		Golem->PurchasedSkills.Reset();
	}
	else if (Command.Operation >= EGolemRPGOperation::ApplyBuild
		&& Command.Operation <= EGolemRPGOperation::ApplyTalentLoadout)
	{
		const FGolemResult Result = BuildSkillCandidate(*Golem, Existing, InventoryView, Command);
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	else
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RPGCandidate"),
			LOCTEXT("Operation", "The RPG operation is not supported."));
	}
	if (Existing.IsItemizationEnabled())
	{
		const FGolemResult Result = SynchronizeLocations(OutView, InventoryView, OutInventoryRequest);
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	++Golem->Revision;
	++OutView.Revision;
	SortView(OutView);
	return OutInventoryRequest.Mutations.IsEmpty() ? FGolemResult::Success() : OutInventoryRequest.ValidateShape();
}

FGolemResult UGolemRPGComponent::BuildProjection(FGolemRPGGolemView& Golem,
	const TArray<FGolemItemInstance>& Items, FGolemGASProjectionPlan& OutPlan) const
{
	OutPlan = FGolemGASProjectionPlan();
	OutPlan.SourceKey = GolemRPG::ProjectionSource;
	OutPlan.DefinitionId = Golem.HeroDefinitionId;
	OutPlan.SkillBuild = Golem.Build;
	OutPlan.EquipmentCombatBuild.bEnabled = Golem.bGearEnabled;
	if (Golem.bGearEnabled)
	{
		if (Golem.ConsumableBindings.Num() != 2)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("QuickBindings"),
				LOCTEXT("QuickBindings", "Gear Golems retain two explicit definition bindings."));
		}
		for (const FPrimaryAssetId& Binding : Golem.ConsumableBindings)
		{
			const UGolemDefinition* Definition = nullptr;
			const FGolemResult Result = ResolveDefinition(Binding, UGolemItemDefinition::StaticClass(), Definition);
			if (!Result.IsSuccess())
			{
				return Result;
			}
			const FGolemItemMetadata& Metadata = CastChecked<UGolemItemDefinition>(Definition)->GetMetadata();
			if (Metadata.Category != EGolemItemCategory::Consumable
				|| (Metadata.SocketProfile != TEXT("Health") && Metadata.SocketProfile != TEXT("Resource")))
			{
				return FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("QuickBindingEffect"),
					LOCTEXT("QuickBindingEffect", "Bind only actual supported health or resource consumable definitions."));
			}
		}
	}
	if (!Golem.Build.SpecId.IsNone())
	{
		const FGolemResult BuildResult = FGolemSkillCatalog::ValidateBuild(Golem.HeroDefinitionId, Golem.Build);
		if (!BuildResult.IsSuccess())
		{
			return BuildResult;
		}
	}
	if (Golem.TalentLoadouts.Num() > GolemRPG::MaximumTalentLoadouts)
	{
		return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("TalentLoadouts"),
			LOCTEXT("PresetCapacity", "The talent loadout collection exceeds its finite capacity."));
	}
	TSet<FGuid> PresetIds;
	TSet<FString> PresetNames;
	for (const FGolemTalentLoadout& Preset : Golem.TalentLoadouts)
	{
		if (!Preset.LoadoutId.IsValid() || PresetIds.Contains(Preset.LoadoutId) || Preset.Version < 1
			|| Preset.Name.IsEmpty() || Preset.Name.Len() > GolemRPG::MaximumLoadoutNameLength
			|| Preset.Name != Preset.Name.TrimStartAndEnd() || PresetNames.Contains(Preset.Name.ToLower())
			|| Preset.Build.SpecId.IsNone() || !Preset.Build.WeaponRune.IsNone() || !Preset.Build.SkillRune.IsNone()
			|| !FGolemSkillCatalog::ValidateBuild(Golem.HeroDefinitionId, Preset.Build).IsSuccess())
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("TalentLoadouts"),
				LOCTEXT("PresetData", "Saved talent loadouts need distinct identities, valid names and rune-free builds for their canonical Golem."));
		}
		for (TCHAR Character : Preset.Name)
		{
			if (FChar::IsControl(Character))
			{
				return FGolemResult::Failure(EGolemError::InvalidData, TEXT("TalentLoadoutName"),
					LOCTEXT("PresetControl", "Talent loadout names cannot contain control characters."));
			}
		}
		PresetIds.Add(Preset.LoadoutId);
		PresetNames.Add(Preset.Name.ToLower());
	}
	if (Golem.ActiveTalentLoadoutId.IsValid() && !PresetIds.Contains(Golem.ActiveTalentLoadoutId))
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("ActiveTalentLoadout"),
			LOCTEXT("ActivePreset", "The selected talent loadout identity must exist for this Golem."));
	}
	FGolemItemStatBonuses Bonuses;
	FGolemRuneTuning Rune;
	if (Golem.Build.ClassChoice == TEXT("Endurance"))
	{
		Bonuses.MaxStamina += 10.0f;
	}
	else if (Golem.Build.ClassChoice == TEXT("Reservoir"))
	{
		Bonuses.MaxRuneEnergy += 10.0f;
	}
	auto AddTuning = [&Bonuses, &Rune, &OutPlan](const FGolemItemStatBonuses& InBonuses,
		const FGolemRuneTuning& InRune, const FPrimaryAssetId& AbilitySetId)
	{
		Bonuses.Add(InBonuses);
		Rune.DamageScale *= InRune.DamageScale;
		Rune.CostScale *= InRune.CostScale;
		Rune.DurationScale *= InRune.DurationScale;
		if (AbilitySetId != FPrimaryAssetId())
		{
			OutPlan.OptionalAbilitySetIds.Add(AbilitySetId);
		}
	};
	TSet<FGameplayTag> Slots;
	TSet<FGuid> AssignedIds;
	TSet<FPrimaryAssetId> UniqueGroups;
	for (const FGolemEquipmentAssignment& Assignment : Golem.Assignments)
	{
		const FGolemItemInstance* Item = Items.FindByPredicate([&Assignment](const FGolemItemInstance& Entry)
			{
				return Entry.InstanceId == Assignment.ItemInstanceId && Entry.DefinitionId == Assignment.DefinitionId;
			});
		if (Item == nullptr || Item->Count != 1 || Slots.Contains(Assignment.SlotTag)
			|| AssignedIds.Contains(Assignment.ItemInstanceId))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RPGProjection"),
				LOCTEXT("ProjectionAssignment", "Every equipment assignment must name one distinct existing unstackable instance."));
		}
		const UGolemDefinition* Resolved = nullptr;
		FGolemResult Result = ResolveDefinition(Item->DefinitionId, UGolemItemDefinition::StaticClass(), Resolved);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		const UGolemItemDefinition* Definition = CastChecked<UGolemItemDefinition>(Resolved);
		Result = Definition->ValidateItem();
		if (!Result.IsSuccess())
		{
			return Result;
		}
		const UGolemRuneDefinition* RuneDefinition = Cast<UGolemRuneDefinition>(Definition);
		const FGolemItemMetadata& Metadata = Definition->GetMetadata();
		if ((Definition->GetItemKind() == EGolemItemKind::Equipment
			&& (!Metadata.AllowedSlots.Contains(Assignment.SlotTag)
				|| Golem.bGearEnabled && Golem.Level < Metadata.RequiredLevel))
			|| (Definition->GetItemKind() == EGolemItemKind::Rune
				&& (Assignment.SlotTag != GolemGameplayTags::Slot_Equipment_Rune || RuneDefinition == nullptr))
			|| (Definition->GetItemKind() != EGolemItemKind::Equipment && Definition->GetItemKind() != EGolemItemKind::Rune))
		{
			return FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("RPGProjection"),
				LOCTEXT("ProjectionSlot", "An equipment assignment has an incompatible native slot or item kind."));
		}
		if (Metadata.UniqueEquipLimit > 0 && UniqueGroups.Contains(Metadata.UniqueEquipGroup))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("UniqueEquipped"),
				LOCTEXT("UniqueRing", "Each unique-equipped ring definition may be equipped once per canonical Golem."));
		}
		if (Metadata.UniqueEquipLimit > 0)
		{
			UniqueGroups.Add(Metadata.UniqueEquipGroup);
		}
		if (Assignment.SlotTag == GolemGameplayTags::Slot_Equipment_Weapon)
		{
			OutPlan.EquipmentCombatBuild.bHasHammer = Golem.bGearEnabled
				&& Metadata.Subtype == EGolemEquipmentSubtype::Hammer;
		}
		if (Assignment.SlotTag == GolemGameplayTags::Slot_Equipment_Relic && Golem.bGearEnabled)
		{
			OutPlan.EquipmentCombatBuild.RelicInstanceId = Item->InstanceId;
			OutPlan.EquipmentCombatBuild.RelicEffectId = Metadata.EquipEffectId;
		}
		AddTuning(Definition->GetEffectiveStats(Item->UpgradeRank),
			RuneDefinition == nullptr ? FGolemRuneTuning() : RuneDefinition->GetRuneTuning(), Definition->GetAbilitySetId());
		Slots.Add(Assignment.SlotTag);
		AssignedIds.Add(Assignment.ItemInstanceId);
	}
	TSet<FPrimaryAssetId> OwnedSkills;
	for (const FGolemPurchasedSkill& Purchase : Golem.PurchasedSkills)
	{
		const TObjectPtr<UGolemSkillDefinition>* Pointer = Skills.Find(Purchase.DefinitionId);
		const UGolemSkillDefinition* Skill = Pointer == nullptr ? nullptr : Pointer->Get();
		if (!IsValid(Skill) || OwnedSkills.Contains(Purchase.DefinitionId) || Purchase.PaidPoints < 1
			|| Purchase.PaidPoints > GolemRPG::MaximumSkillPoints || Purchase.PaidMaterials.Num() > GolemItems::MaximumBatchOperations)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RPGProjection"),
				LOCTEXT("ProjectionSkill", "Purchased skills require distinct actual graph nodes and retained valid paid costs."));
		}
		for (const FPrimaryAssetId& Required : Skill->GetPrerequisites())
		{
			if (!Golem.HasSkill(Required))
			{
				return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RPGProjection"),
					LOCTEXT("ProjectionPrerequisite", "A purchased skill is missing its retained prerequisite."));
			}
		}
		for (const FGolemItemInstance& Paid : Purchase.PaidMaterials)
		{
			if (!Paid.ValidateIdentityAndCount().IsSuccess())
			{
				return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RPGProjection"),
					LOCTEXT("ProjectionPaidMaterial", "Retained material cost identities and counts are invalid."));
			}
		}
		AddTuning(Skill->GetStatBonuses(), Skill->GetRuneTuning(), Skill->GetAbilitySetId());
		OwnedSkills.Add(Purchase.DefinitionId);
	}
	FGolemResult Result = Bonuses.Validate();
	if (Result.IsSuccess())
	{
		Result = Rune.Validate();
	}
	if (!Result.IsSuccess())
	{
		return Result;
	}
	OutPlan.MaxHealthBonus = Bonuses.MaxHealth;
	OutPlan.MaxStaminaBonus = Bonuses.MaxStamina;
	OutPlan.MaxRuneEnergyBonus = Bonuses.MaxRuneEnergy;
	OutPlan.AttackPowerBonus = Bonuses.AttackPower;
	OutPlan.DefenseBonus = Bonuses.Defense;
	OutPlan.CritChanceBonus = Bonuses.CritChance;
	OutPlan.HasteBonus = Bonuses.Haste;
	OutPlan.CooldownRecoveryBonus = Bonuses.CooldownRecovery;
	OutPlan.MasteryBonus = Bonuses.MasteryBonus;
	OutPlan.VersatilityBonus = Bonuses.Versatility;
	OutPlan.MaxPoiseBonus = Bonuses.Poise;
	if (!(Rune == FGolemRuneTuning()))
	{
		FGolemActionModifier Modifier;
		Modifier.AbilityTag = GolemGameplayTags::Ability_Combat_Rune;
		Modifier.DamageScale = Rune.DamageScale;
		Modifier.CostScale = Rune.CostScale;
		Modifier.DurationScale = Rune.DurationScale;
		OutPlan.ActionModifiers.Add(Modifier);
	}
	OutPlan.OptionalAbilitySetIds.Sort([](const FPrimaryAssetId& Left, const FPrimaryAssetId& Right)
		{
			return Left.ToString() < Right.ToString();
		});
	Result = OutPlan.Validate();
	if (Result.IsSuccess())
	{
		Golem.StatBonuses = Bonuses;
		Golem.RuneTuning = Rune;
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
