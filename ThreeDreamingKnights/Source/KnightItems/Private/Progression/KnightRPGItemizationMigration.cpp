#include "Progression/KnightRPGComponent.h"

#include "Catalog/KnightGearCatalog.h"
#include "Definitions/KnightItemDefinition.h"
#include "Inventory/KnightInventoryComponent.h"

#define LOCTEXT_NAMESPACE "KnightRPGItemizationMigration"

FKnightResult UKnightRPGComponent::BuildItemizationMigration(const FKnightRPGView& Existing,
	const FKnightInventoryView& InventoryView, const FGuid& MigrationId,
	FKnightRPGView& OutView, FKnightInventoryRequest& OutRequest) const
{
	OutView = FKnightRPGView();
	OutRequest = FKnightInventoryRequest();
	if (!MigrationId.IsValid() || Inventory == nullptr || !Existing.IsInitialized()
		|| !InventoryView.IsInitialized() || Existing.IsItemizationEnabled() || Existing.bGearDemo
		|| Existing.Currency != 0 || !Existing.ClaimedRewards.IsEmpty()
		|| Existing.Revision >= MAX_int64 || InventoryView.Revision >= MAX_int64)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("ItemizationLegacyState"),
			LOCTEXT("LegacyState", "Itemization import requires one verified legacy state without demo grants."));
	}
	FKnightRPGView Candidate = Existing;
	FKnightInventoryRequest Request;
	Request.RequestId = MigrationId;
	Request.ExpectedRevision = InventoryView.Revision;
	for (FKnightRPGKnightView& Knight : Candidate.Knights)
	{
		if (Knight.bGearEnabled || Knight.Revision >= MAX_int64 || Knight.Level != 1)
		{
			return FKnightResult::Failure(EKnightError::InvalidState, TEXT("ItemizationLegacyKnight"),
				LOCTEXT("LegacyKnight", "All imported Knights must retain their original legacy level and identity."));
		}
		TArray<FKnightEquipmentAssignment> Mapped;
		for (const FKnightEquipmentAssignment& Assignment : Knight.Assignments)
		{
			const FKnightItemInstance* Item = InventoryView.Items.FindByPredicate(
				[&Assignment](const FKnightItemInstance& Entry)
				{
					return Entry.InstanceId == Assignment.ItemInstanceId && Entry.DefinitionId == Assignment.DefinitionId;
				});
			if (Item == nullptr || Item->Count != 1)
			{
				return FKnightResult::Failure(EKnightError::NotFound, TEXT("ItemizationLegacyInstance"),
					LOCTEXT("LegacyInstance", "An imported assignment must identify its actual retained unstackable instance."));
			}
			const UKnightDefinition* Resolved = nullptr;
			const FKnightResult Resolution = ResolveDefinition(Item->DefinitionId, UKnightItemDefinition::StaticClass(), Resolved);
			if (!Resolution.IsSuccess())
			{
				return Resolution;
			}
			const UKnightItemDefinition* Definition = CastChecked<UKnightItemDefinition>(Resolved);
			if (KnightGear::IsEquipmentSlot(Assignment.SlotTag)
				&& Definition->GetItemKind() == EKnightItemKind::Equipment
				&& Definition->GetMetadata().AllowedSlots.Contains(Assignment.SlotTag))
			{
				Mapped.Add(Assignment);
			}
			// Unmapped legacy slots withdraw their source and leave the same instance in the shared bag.
		}
		Knight.Assignments = MoveTemp(Mapped);
		Knight.bGearEnabled = true;
		Knight.ConsumableBindings = { FKnightGearCatalog::DefinitionId(TEXT("C01")),
			FKnightGearCatalog::DefinitionId(TEXT("C02")) };
		++Knight.Revision;
	}
	FKnightResult Result = SynchronizeLocations(Candidate, InventoryView, Request);
	if (Result.IsSuccess() && !Request.Mutations.IsEmpty())
	{
		Result = Request.ValidateShape();
	}
	if (!Result.IsSuccess())
	{
		return Result;
	}
	TSet<FGuid> UsedIds;
	for (const FKnightItemInstance& Item : InventoryView.Items)
	{
		UsedIds.Add(Item.InstanceId);
	}
	TArray<FKnightItemInstance> Items;
	TSet<FGuid> CandidateIds;
	Result = Inventory->BuildCandidate(InventoryView.Items, UsedIds, InventoryView.Capacity, Request, Items, CandidateIds);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	for (FKnightRPGKnightView& Knight : Candidate.Knights)
	{
		FKnightGASProjectionPlan Plan;
		Result = BuildProjection(Knight, Items, Plan);
		if (Result.IsSuccess())
		{
			Result = ValidateBuildAttachments(Knight, Candidate, Items);
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
	return FKnightResult::Success();
}

FKnightResult UKnightRPGComponent::AppendItemizationMigration(FKnightRPGSnapshot& Snapshot,
	const FGuid& MigrationId) const
{
	if (Snapshot.ItemizationMigration.IsPresent() || Snapshot.View.IsItemizationEnabled()
		|| Snapshot.Receipts.ContainsByPredicate([&MigrationId](const FKnightRPGReceipt& Receipt)
		{
			return Receipt.Command.RequestId == MigrationId;
		}) || Snapshot.Inventory.Receipts.ContainsByPredicate([&MigrationId](const FKnightInventoryReceipt& Receipt)
		{
			return Receipt.Request.RequestId == MigrationId;
		}))
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("ItemizationMigrationReplay"),
			LOCTEXT("MigrationReplay", "Import itemization once, using an identity absent from every historical request."));
	}
	FKnightInventoryView InventoryView;
	InventoryView.InventoryId = Snapshot.Inventory.InventoryId;
	InventoryView.Capacity = Snapshot.Inventory.Capacity;
	InventoryView.Revision = Snapshot.Inventory.Revision;
	InventoryView.Items = Snapshot.Inventory.Items;
	FKnightRPGView Candidate;
	FKnightInventoryRequest Request;
	FKnightResult Result = BuildItemizationMigration(Snapshot.View, InventoryView, MigrationId, Candidate, Request);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!Request.Mutations.IsEmpty())
	{
		if (Snapshot.Inventory.Receipts.Num() >= KnightItems::MaximumTransactionReceipts)
		{
			return FKnightResult::Failure(EKnightError::CapacityExceeded, TEXT("ItemizationJournalCapacity"),
				LOCTEXT("JournalCapacity", "Migration retains every receipt and requires space for its new location receipt."));
		}
		TArray<FKnightItemInstance> ExistingItems;
		TSet<FGuid> UsedIds;
		TMap<FGuid, int32> MigrationReceiptIndices;
		Result = Inventory->ValidateSnapshot(Snapshot.Inventory, ExistingItems, UsedIds, MigrationReceiptIndices);
		TArray<FKnightItemInstance> Items;
		TSet<FGuid> CandidateIds;
		if (Result.IsSuccess())
		{
			Result = Inventory->BuildCandidate(ExistingItems, UsedIds, InventoryView.Capacity, Request, Items, CandidateIds);
		}
		if (!Result.IsSuccess())
		{
			return Result;
		}
		FKnightInventoryReceipt Receipt;
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
	return FKnightResult::Success();
}

#undef LOCTEXT_NAMESPACE
