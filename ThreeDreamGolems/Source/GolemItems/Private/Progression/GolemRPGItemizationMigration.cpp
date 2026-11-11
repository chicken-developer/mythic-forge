#include "Progression/GolemRPGComponent.h"

#include "Catalog/GolemGearCatalog.h"
#include "Definitions/GolemItemDefinition.h"
#include "Inventory/GolemInventoryComponent.h"

#define LOCTEXT_NAMESPACE "GolemRPGItemizationMigration"

FGolemResult UGolemRPGComponent::BuildItemizationMigration(const FGolemRPGView& Existing,
	const FGolemInventoryView& InventoryView, const FGuid& MigrationId,
	FGolemRPGView& OutView, FGolemInventoryRequest& OutRequest) const
{
	OutView = FGolemRPGView();
	OutRequest = FGolemInventoryRequest();
	if (!MigrationId.IsValid() || Inventory == nullptr || !Existing.IsInitialized()
		|| !InventoryView.IsInitialized() || Existing.IsItemizationEnabled() || Existing.bGearDemo
		|| Existing.Currency != 0 || !Existing.ClaimedRewards.IsEmpty()
		|| Existing.Revision >= MAX_int64 || InventoryView.Revision >= MAX_int64)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("ItemizationLegacyState"),
			LOCTEXT("LegacyState", "Itemization import requires one verified legacy state without demo grants."));
	}
	FGolemRPGView Candidate = Existing;
	FGolemInventoryRequest Request;
	Request.RequestId = MigrationId;
	Request.ExpectedRevision = InventoryView.Revision;
	for (FGolemRPGGolemView& Golem : Candidate.Golems)
	{
		if (Golem.bGearEnabled || Golem.Revision >= MAX_int64 || Golem.Level != 1)
		{
			return FGolemResult::Failure(EGolemError::InvalidState, TEXT("ItemizationLegacyGolem"),
				LOCTEXT("LegacyGolem", "All imported Golems must retain their original legacy level and identity."));
		}
		TArray<FGolemEquipmentAssignment> Mapped;
		for (const FGolemEquipmentAssignment& Assignment : Golem.Assignments)
		{
			const FGolemItemInstance* Item = InventoryView.Items.FindByPredicate(
				[&Assignment](const FGolemItemInstance& Entry)
				{
					return Entry.InstanceId == Assignment.ItemInstanceId && Entry.DefinitionId == Assignment.DefinitionId;
				});
			if (Item == nullptr || Item->Count != 1)
			{
				return FGolemResult::Failure(EGolemError::NotFound, TEXT("ItemizationLegacyInstance"),
					LOCTEXT("LegacyInstance", "An imported assignment must identify its actual retained unstackable instance."));
			}
			const UGolemDefinition* Resolved = nullptr;
			const FGolemResult Resolution = ResolveDefinition(Item->DefinitionId, UGolemItemDefinition::StaticClass(), Resolved);
			if (!Resolution.IsSuccess())
			{
				return Resolution;
			}
			const UGolemItemDefinition* Definition = CastChecked<UGolemItemDefinition>(Resolved);
			if (GolemGear::IsEquipmentSlot(Assignment.SlotTag)
				&& Definition->GetItemKind() == EGolemItemKind::Equipment
				&& Definition->GetMetadata().AllowedSlots.Contains(Assignment.SlotTag))
			{
				Mapped.Add(Assignment);
			}
			// Unmapped legacy slots withdraw their source and leave the same instance in the shared bag.
		}
		Golem.Assignments = MoveTemp(Mapped);
		Golem.bGearEnabled = true;
		Golem.ConsumableBindings = { FGolemGearCatalog::DefinitionId(TEXT("C01")),
			FGolemGearCatalog::DefinitionId(TEXT("C02")) };
		++Golem.Revision;
	}
	FGolemResult Result = SynchronizeLocations(Candidate, InventoryView, Request);
	if (Result.IsSuccess() && !Request.Mutations.IsEmpty())
	{
		Result = Request.ValidateShape();
	}
	if (!Result.IsSuccess())
	{
		return Result;
	}
	TSet<FGuid> UsedIds;
	for (const FGolemItemInstance& Item : InventoryView.Items)
	{
		UsedIds.Add(Item.InstanceId);
	}
	TArray<FGolemItemInstance> Items;
	TSet<FGuid> CandidateIds;
	Result = Inventory->BuildCandidate(InventoryView.Items, UsedIds, InventoryView.Capacity, Request, Items, CandidateIds);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	for (FGolemRPGGolemView& Golem : Candidate.Golems)
	{
		FGolemGASProjectionPlan Plan;
		Result = BuildProjection(Golem, Items, Plan);
		if (Result.IsSuccess())
		{
			Result = ValidateBuildAttachments(Golem, Candidate, Items);
		}
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	Result = ValidateGearLocations(Candidate, Items);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	++Candidate.Revision;
	Candidate.InventoryRevision = InventoryView.Revision + (Request.Mutations.IsEmpty() ? 0 : 1);
	SortView(Candidate);
	OutView = MoveTemp(Candidate);
	OutRequest = MoveTemp(Request);
	return FGolemResult::Success();
}

FGolemResult UGolemRPGComponent::AppendItemizationMigration(FGolemRPGSnapshot& Snapshot,
	const FGuid& MigrationId) const
{
	if (Snapshot.ItemizationMigration.IsPresent() || Snapshot.View.IsItemizationEnabled()
		|| Snapshot.Receipts.ContainsByPredicate([&MigrationId](const FGolemRPGReceipt& Receipt)
		{
			return Receipt.Command.RequestId == MigrationId;
		}) || Snapshot.Inventory.Receipts.ContainsByPredicate([&MigrationId](const FGolemInventoryReceipt& Receipt)
		{
			return Receipt.Request.RequestId == MigrationId;
		}))
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("ItemizationMigrationReplay"),
			LOCTEXT("MigrationReplay", "Import itemization once, using an identity absent from every historical request."));
	}
	FGolemInventoryView InventoryView;
	InventoryView.InventoryId = Snapshot.Inventory.InventoryId;
	InventoryView.Capacity = Snapshot.Inventory.Capacity;
	InventoryView.Revision = Snapshot.Inventory.Revision;
	InventoryView.Items = Snapshot.Inventory.Items;
	FGolemRPGView Candidate;
	FGolemInventoryRequest Request;
	FGolemResult Result = BuildItemizationMigration(Snapshot.View, InventoryView, MigrationId, Candidate, Request);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!Request.Mutations.IsEmpty())
	{
		if (Snapshot.Inventory.Receipts.Num() >= GolemItems::MaximumTransactionReceipts)
		{
			return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("ItemizationJournalCapacity"),
				LOCTEXT("JournalCapacity", "Migration retains every receipt and requires space for its new location receipt."));
		}
		TArray<FGolemItemInstance> ExistingItems;
		TSet<FGuid> UsedIds;
		TMap<FGuid, int32> MigrationReceiptIndices;
		Result = Inventory->ValidateSnapshot(Snapshot.Inventory, ExistingItems, UsedIds, MigrationReceiptIndices);
		TArray<FGolemItemInstance> Items;
		TSet<FGuid> CandidateIds;
		if (Result.IsSuccess())
		{
			Result = Inventory->BuildCandidate(ExistingItems, UsedIds, InventoryView.Capacity, Request, Items, CandidateIds);
		}
		if (!Result.IsSuccess())
		{
			return Result;
		}
		FGolemInventoryReceipt Receipt;
		Receipt.Request = Request;
		Receipt.CommittedRevision = InventoryView.Revision + 1;
		Snapshot.Inventory.Receipts.Add(MoveTemp(Receipt));
		Snapshot.Inventory.Items = MoveTemp(Items);
		++Snapshot.Inventory.Revision;
	}
	Snapshot.ItemizationMigration.MigrationId = MigrationId;
	Snapshot.ItemizationMigration.LegacyReceiptCount = Snapshot.Receipts.Num();
	Snapshot.ItemizationMigration.InventoryRequest = MoveTemp(Request);
	Snapshot.LegacyWeaponRuneParents.Reset();
	Snapshot.View = MoveTemp(Candidate);
	return FGolemResult::Success();
}

#undef LOCTEXT_NAMESPACE
