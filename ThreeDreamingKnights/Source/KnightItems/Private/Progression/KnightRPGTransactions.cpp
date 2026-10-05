#include "Progression/KnightRPGComponent.h"

#include "Assets/KnightDefinition.h"
#include "Definitions/KnightItemDefinition.h"
#include "Definitions/KnightRuneDefinition.h"
#include "Definitions/KnightSkillDefinition.h"
#include "Tags/KnightGameplayTags.h"

#define LOCTEXT_NAMESPACE "KnightRPGTransactions"

FKnightResult UKnightRPGComponent::BuildCandidate(const FKnightRPGView& Existing,
	const FKnightInventoryView& InventoryView, const FKnightRPGCommand& Command,
	const FKnightItemInstance* PickedItem, const TArray<FKnightInventoryMutation>* HistoricalMutations,
	FKnightRPGView& OutView, FKnightInventoryRequest& OutInventoryRequest, FGuid& OutAffectedInstance) const
{
	OutView = Existing;
	OutInventoryRequest = FKnightInventoryRequest();
	OutInventoryRequest.RequestId = Command.RequestId;
	OutInventoryRequest.ExpectedRevision = Command.ExpectedInventoryRevision;
	OutAffectedInstance.Invalidate();
	FKnightRPGKnightView* Knight = OutView.Knights.FindByPredicate([&Command](const FKnightRPGKnightView& Entry)
		{
			return Entry.KnightId == Command.ExpectedKnightId;
		});
	if (Knight == nullptr || !Knight->IsInitialized() || Command.InventoryId != InventoryView.InventoryId
		|| Command.ExpectedInventoryRevision != InventoryView.Revision || Command.ExpectedKnightRevision != Knight->Revision
		|| Existing.Revision >= MAX_int64 || Knight->Revision >= MAX_int64)
	{
		return FKnightResult::Failure(EKnightError::InvalidRevision, TEXT("RPGCandidate"),
			LOCTEXT("CandidateRevision", "A candidate must start from exact current inventory and Knight revisions."));
	}
	TSet<FGuid> AssignedIds;
	for (const FKnightRPGKnightView& ExistingKnight : Existing.Knights)
	{
		for (const FKnightEquipmentAssignment& Assignment : ExistingKnight.Assignments)
		{
			if (!Assignment.ItemInstanceId.IsValid() || AssignedIds.Contains(Assignment.ItemInstanceId))
			{
				return FKnightResult::Failure(EKnightError::DuplicateId, TEXT("RPGAssignments"),
					LOCTEXT("AssignmentDuplicate", "An item instance may be assigned to only one canonical Knight and slot."));
			}
			AssignedIds.Add(Assignment.ItemInstanceId);
		}
	}
	if (Command.Operation == EKnightRPGOperation::Pickup)
	{
		if (PickedItem == nullptr || !PickedItem->ValidateIdentityAndCount().IsSuccess())
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGPickupCandidate"),
				LOCTEXT("PickedItem", "A pickup requires the exact validated item from its reserved authoritative actor."));
		}
		FKnightInventoryMutation Mutation;
		Mutation.Item = *PickedItem;
		OutInventoryRequest.Mutations.Add(Mutation);
		OutAffectedInstance = PickedItem->InstanceId;
	}
	else if (Command.Operation == EKnightRPGOperation::Equip || Command.Operation == EKnightRPGOperation::Rune)
	{
		const FKnightItemInstance* Item = InventoryView.Items.FindByPredicate([&Command](const FKnightItemInstance& Entry)
			{
				return Entry.InstanceId == Command.ItemInstanceId;
			});
		if (Item == nullptr)
		{
			return FKnightResult::Failure(EKnightError::NotFound, TEXT("RPGEquip"),
				LOCTEXT("EquipInstance", "The exact equipment instance is not present in shared inventory."));
		}
		const UKnightDefinition* Resolved = nullptr;
		const FKnightResult Resolution = ResolveDefinition(Item->DefinitionId, UKnightItemDefinition::StaticClass(), Resolved);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		const UKnightItemDefinition* Definition = CastChecked<UKnightItemDefinition>(Resolved);
		const FKnightResult Validation = Definition->ValidateItem();
		if (!Validation.IsSuccess())
		{
			return Validation;
		}
		const bool bRune = Command.Operation == EKnightRPGOperation::Rune;
		if (Item->Count != 1 || (bRune && (Definition->GetItemKind() != EKnightItemKind::Rune
			|| !Definition->IsA<UKnightRuneDefinition>() || Command.SlotTag != KnightGameplayTags::Slot_Equipment_Rune))
			|| (!bRune && (Definition->GetItemKind() != EKnightItemKind::Equipment
				|| Command.SlotTag != KnightGameplayTags::Slot_Equipment_Weapon)))
		{
			return FKnightResult::Failure(EKnightError::TypeMismatch, TEXT("RPGEquipSlot"),
				LOCTEXT("SlotKind", "The selected item kind must match its native weapon or rune slot."));
		}
		if (AssignedIds.Contains(Item->InstanceId))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGEquipAssignment"),
				LOCTEXT("AlreadyAssigned", "The equipment instance is already assigned; unequip its current owner first."));
		}
		Knight->Assignments.RemoveAll([&Command](const FKnightEquipmentAssignment& Assignment)
			{
				return Assignment.SlotTag == Command.SlotTag;
			});
		FKnightEquipmentAssignment Assignment;
		Assignment.SlotTag = Command.SlotTag;
		Assignment.ItemInstanceId = Item->InstanceId;
		Assignment.DefinitionId = Item->DefinitionId;
		Knight->Assignments.Add(Assignment);
		OutAffectedInstance = Item->InstanceId;
	}
	else if (Command.Operation == EKnightRPGOperation::Unequip)
	{
		const FKnightEquipmentAssignment* Assignment = Knight->FindAssignment(Command.SlotTag);
		if (Assignment == nullptr || Assignment->ItemInstanceId != Command.ItemInstanceId)
		{
			return FKnightResult::Failure(EKnightError::NotOwned, TEXT("RPGUnequip"),
				LOCTEXT("UnequipIdentity", "Unequip requires this Knight's exact currently assigned slot and instance."));
		}
		OutAffectedInstance = Assignment->ItemInstanceId;
		Knight->Assignments.RemoveAll([&Command](const FKnightEquipmentAssignment& Entry)
			{
				return Entry.SlotTag == Command.SlotTag;
			});
	}
	else if (Command.Operation == EKnightRPGOperation::PurchaseSkill)
	{
		const TObjectPtr<UKnightSkillDefinition>* SkillPointer = Skills.Find(Command.SkillDefinitionId);
		const UKnightSkillDefinition* Skill = SkillPointer == nullptr ? nullptr : SkillPointer->Get();
		if (!IsValid(Skill))
		{
			return FKnightResult::Failure(EKnightError::NotFound, TEXT("RPGPurchaseSkill"),
				LOCTEXT("SkillNotInTree", "The requested actual skill is not a validated node in this campaign's skill tree."));
		}
		if (Knight->HasSkill(Command.SkillDefinitionId))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGPurchaseSkill"),
				LOCTEXT("SkillOwned", "This Knight already purchased the exact skill."));
		}
		for (const FPrimaryAssetId& Required : Skill->GetPrerequisites())
		{
			if (!Knight->HasSkill(Required))
			{
				return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGPurchaseSkill"),
					LOCTEXT("Prerequisite", "Purchase the skill's actual prerequisites first."));
			}
		}
		if (Knight->SkillPoints < Skill->GetPointCost())
		{
			return FKnightResult::Failure(EKnightError::InsufficientResources, TEXT("RPGSkillPoints"),
				LOCTEXT("Points", "This Knight does not have enough skill points."));
		}
		FKnightPurchasedSkill Purchase;
		Purchase.DefinitionId = Command.SkillDefinitionId;
		Purchase.PaidPoints = Skill->GetPointCost();
		TArray<FKnightItemInstance> SortedItems = InventoryView.Items;
		SortedItems.Sort([](const FKnightItemInstance& Left, const FKnightItemInstance& Right)
			{
				return Left.InstanceId < Right.InstanceId;
			});
		for (const FKnightMaterialCost& Cost : Skill->GetMaterialCosts())
		{
			int32 Remaining = Cost.Count;
			for (const FKnightItemInstance& Item : SortedItems)
			{
				if (Remaining == 0)
				{
					break;
				}
				if (Item.DefinitionId != Cost.DefinitionId || AssignedIds.Contains(Item.InstanceId))
				{
					continue;
				}
				FKnightInventoryMutation Mutation;
				Mutation.Operation = EKnightInventoryOperation::Remove;
				Mutation.Item = Item;
				Mutation.Item.Count = FMath::Min(Remaining, Item.Count);
				Remaining -= Mutation.Item.Count;
				OutInventoryRequest.Mutations.Add(Mutation);
				Purchase.PaidMaterials.Add(Mutation.Item);
			}
			if (Remaining > 0)
			{
				return FKnightResult::Failure(EKnightError::InsufficientResources, TEXT("RPGSkillMaterials"),
					LOCTEXT("Materials", "Shared inventory does not contain the complete unassigned material cost."));
			}
		}
		Knight->SkillPoints -= Purchase.PaidPoints;
		Knight->PurchasedSkills.Add(MoveTemp(Purchase));
	}
	else if (Command.Operation == EKnightRPGOperation::Respec)
	{
		if (Knight->PurchasedSkills.IsEmpty())
		{
			return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGRespec"),
				LOCTEXT("NoPaidSkills", "This Knight has no paid skills to refund."));
		}
		TMap<FPrimaryAssetId, int64> Refunds;
		int64 RefundedPoints = Knight->SkillPoints;
		for (const FKnightPurchasedSkill& Purchase : Knight->PurchasedSkills)
		{
			RefundedPoints += Purchase.PaidPoints;
			for (const FKnightItemInstance& Paid : Purchase.PaidMaterials)
			{
				Refunds.FindOrAdd(Paid.DefinitionId) += Paid.Count;
			}
		}
		if (RefundedPoints > KnightRPG::MaximumSkillPoints)
		{
			return FKnightResult::Failure(EKnightError::CapacityExceeded, TEXT("RPGRespec"),
				LOCTEXT("RefundPointBound", "The retained paid point refund exceeds the supported finite balance."));
		}
		TArray<FPrimaryAssetId> Definitions;
		Refunds.GetKeys(Definitions);
		Definitions.Sort([](const FPrimaryAssetId& Left, const FPrimaryAssetId& Right)
			{
				return Left.ToString() < Right.ToString();
			});
		TArray<FKnightItemInstance> SortedItems = InventoryView.Items;
		SortedItems.Sort([](const FKnightItemInstance& Left, const FKnightItemInstance& Right)
			{
				return Left.InstanceId < Right.InstanceId;
			});
		for (const FPrimaryAssetId& Id : Definitions)
		{
			const UKnightDefinition* Resolved = nullptr;
			const FKnightResult Resolution = ResolveDefinition(Id, UKnightItemDefinition::StaticClass(), Resolved);
			if (!Resolution.IsSuccess())
			{
				return Resolution;
			}
			const UKnightItemDefinition* Definition = CastChecked<UKnightItemDefinition>(Resolved);
			if (Definition->GetItemKind() != EKnightItemKind::Material || !Definition->ValidateItem().IsSuccess())
			{
				return FKnightResult::Failure(EKnightError::TypeMismatch, TEXT("RPGRefund"),
					LOCTEXT("RefundMaterial", "Retained material refunds require actual validated Material definitions."));
			}
			int64 Remaining = Refunds.FindChecked(Id);
			for (const FKnightItemInstance& Item : SortedItems)
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
				FKnightInventoryMutation Mutation;
				Mutation.Item = Item;
				Mutation.Item.Count = static_cast<int32>(FMath::Min<int64>(Remaining, Room));
				Remaining -= Mutation.Item.Count;
				OutInventoryRequest.Mutations.Add(Mutation);
			}
			while (Remaining > 0)
			{
				FKnightInventoryMutation Mutation;
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
				if (OutInventoryRequest.Mutations.Num() > KnightItems::MaximumBatchOperations)
				{
					return FKnightResult::Failure(EKnightError::CapacityExceeded, TEXT("RPGRefund"),
						LOCTEXT("RefundBatch", "The retained refund exceeds the supported finite atomic mutation batch."));
				}
			}
		}
		Knight->SkillPoints = static_cast<int32>(RefundedPoints);
		Knight->PurchasedSkills.Reset();
	}
	else
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGCandidate"),
			LOCTEXT("Operation", "The RPG operation is not supported."));
	}
	++Knight->Revision;
	++OutView.Revision;
	SortView(OutView);
	return OutInventoryRequest.Mutations.IsEmpty() ? FKnightResult::Success() : OutInventoryRequest.ValidateShape();
}

FKnightResult UKnightRPGComponent::BuildProjection(FKnightRPGKnightView& Knight,
	const TArray<FKnightItemInstance>& Items, FKnightGASProjectionPlan& OutPlan) const
{
	OutPlan = FKnightGASProjectionPlan();
	OutPlan.SourceKey = KnightRPG::ProjectionSource;
	OutPlan.DefinitionId = Knight.HeroDefinitionId;
	FKnightItemStatBonuses Bonuses;
	FKnightRuneTuning Rune;
	auto AddTuning = [&Bonuses, &Rune, &OutPlan](const FKnightItemStatBonuses& InBonuses,
		const FKnightRuneTuning& InRune, const FPrimaryAssetId& AbilitySetId)
	{
		Bonuses.MaxHealth += InBonuses.MaxHealth;
		Bonuses.MaxStamina += InBonuses.MaxStamina;
		Bonuses.MaxRuneEnergy += InBonuses.MaxRuneEnergy;
		Bonuses.AttackPower += InBonuses.AttackPower;
		Bonuses.Defense += InBonuses.Defense;
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
	for (const FKnightEquipmentAssignment& Assignment : Knight.Assignments)
	{
		const FKnightItemInstance* Item = Items.FindByPredicate([&Assignment](const FKnightItemInstance& Entry)
			{
				return Entry.InstanceId == Assignment.ItemInstanceId && Entry.DefinitionId == Assignment.DefinitionId;
			});
		if (Item == nullptr || Item->Count != 1 || Slots.Contains(Assignment.SlotTag)
			|| AssignedIds.Contains(Assignment.ItemInstanceId))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGProjection"),
				LOCTEXT("ProjectionAssignment", "Every equipment assignment must name one distinct existing unstackable instance."));
		}
		const UKnightDefinition* Resolved = nullptr;
		FKnightResult Result = ResolveDefinition(Item->DefinitionId, UKnightItemDefinition::StaticClass(), Resolved);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		const UKnightItemDefinition* Definition = CastChecked<UKnightItemDefinition>(Resolved);
		Result = Definition->ValidateItem();
		if (!Result.IsSuccess())
		{
			return Result;
		}
		const UKnightRuneDefinition* RuneDefinition = Cast<UKnightRuneDefinition>(Definition);
		if ((Definition->GetItemKind() == EKnightItemKind::Equipment
			&& Assignment.SlotTag != KnightGameplayTags::Slot_Equipment_Weapon)
			|| (Definition->GetItemKind() == EKnightItemKind::Rune
				&& (Assignment.SlotTag != KnightGameplayTags::Slot_Equipment_Rune || RuneDefinition == nullptr))
			|| (Definition->GetItemKind() != EKnightItemKind::Equipment && Definition->GetItemKind() != EKnightItemKind::Rune))
		{
			return FKnightResult::Failure(EKnightError::TypeMismatch, TEXT("RPGProjection"),
				LOCTEXT("ProjectionSlot", "An equipment assignment has an incompatible native slot or item kind."));
		}
		AddTuning(Definition->GetStatBonuses(),
			RuneDefinition == nullptr ? FKnightRuneTuning() : RuneDefinition->GetRuneTuning(), Definition->GetAbilitySetId());
		Slots.Add(Assignment.SlotTag);
		AssignedIds.Add(Assignment.ItemInstanceId);
	}
	TSet<FPrimaryAssetId> OwnedSkills;
	for (const FKnightPurchasedSkill& Purchase : Knight.PurchasedSkills)
	{
		const TObjectPtr<UKnightSkillDefinition>* Pointer = Skills.Find(Purchase.DefinitionId);
		const UKnightSkillDefinition* Skill = Pointer == nullptr ? nullptr : Pointer->Get();
		if (!IsValid(Skill) || OwnedSkills.Contains(Purchase.DefinitionId) || Purchase.PaidPoints < 1
			|| Purchase.PaidPoints > KnightRPG::MaximumSkillPoints || Purchase.PaidMaterials.Num() > KnightItems::MaximumBatchOperations)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGProjection"),
				LOCTEXT("ProjectionSkill", "Purchased skills require distinct actual graph nodes and retained valid paid costs."));
		}
		for (const FPrimaryAssetId& Required : Skill->GetPrerequisites())
		{
			if (!Knight.HasSkill(Required))
			{
				return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGProjection"),
					LOCTEXT("ProjectionPrerequisite", "A purchased skill is missing its retained prerequisite."));
			}
		}
		for (const FKnightItemInstance& Paid : Purchase.PaidMaterials)
		{
			if (!Paid.ValidateIdentityAndCount().IsSuccess())
			{
				return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGProjection"),
					LOCTEXT("ProjectionPaidMaterial", "Retained material cost identities and counts are invalid."));
			}
		}
		AddTuning(Skill->GetStatBonuses(), Skill->GetRuneTuning(), Skill->GetAbilitySetId());
		OwnedSkills.Add(Purchase.DefinitionId);
	}
	FKnightResult Result = Bonuses.Validate();
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
	if (!(Rune == FKnightRuneTuning()))
	{
		FKnightActionModifier Modifier;
		Modifier.AbilityTag = KnightGameplayTags::Ability_Combat_Rune;
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
		Knight.StatBonuses = Bonuses;
		Knight.RuneTuning = Rune;
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
