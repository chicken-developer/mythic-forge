#include "Progression/KnightRPGComponent.h"

#include "Components/KnightAbilitySystemComponent.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Misc/ScopeExit.h"

#define LOCTEXT_NAMESPACE "KnightRPGSnapshots"

FKnightResult UKnightRPGComponent::CaptureSnapshot(FKnightRPGSnapshot& OutSnapshot) const
{
	OutSnapshot = FKnightRPGSnapshot();
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (!IsInitialized() || bTransactionInProgress || Inventory == nullptr || Inventory->IsReserved())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGSnapshot"),
			LOCTEXT("CaptureState", "Capture requires initialized domains outside an in-progress mutation or publication."));
	}
	FKnightRPGSnapshot Candidate;
	Candidate.InitialSkillPoints = InitialSkillPoints;
	Candidate.InitialView = InitialView;
	Candidate.InitialInventory = InitialInventory;
	Candidate.View = PublishedView;
	Candidate.Receipts = Receipts;
	const FKnightResult CaptureResult = Inventory->CaptureSnapshot(Candidate.Inventory);
	if (!CaptureResult.IsSuccess())
	{
		return CaptureResult;
	}
	FKnightRPGView Reconstructed;
	const FKnightResult Validation = ValidateSnapshot(Candidate, Reconstructed);
	if (Validation.IsSuccess())
	{
		OutSnapshot = MoveTemp(Candidate);
	}
	return Validation;
}

FKnightResult UKnightRPGComponent::ValidateSnapshot(const FKnightRPGSnapshot& Snapshot, FKnightRPGView& OutView) const
{
	OutView = FKnightRPGView();
	if (Inventory == nullptr || Snapshot.SchemaVersion != KnightRPG::SnapshotVersion
		|| Snapshot.InitialSkillPoints < 0 || Snapshot.InitialSkillPoints > KnightRPG::MaximumSkillPoints
		|| !Snapshot.InitialView.IsInitialized() || Snapshot.InitialView.Revision != 1
		|| !Snapshot.View.IsInitialized() || Snapshot.View.Revision != static_cast<int64>(Snapshot.Receipts.Num()) + 1
		|| Snapshot.Receipts.Num() > KnightItems::MaximumTransactionReceipts
		|| Snapshot.InitialView.SkillTreeId != Snapshot.View.SkillTreeId
		|| Snapshot.InitialInventory.InventoryId != Snapshot.Inventory.InventoryId
		|| Snapshot.InitialInventory.Capacity != Snapshot.Inventory.Capacity
		|| Snapshot.InitialView.InventoryId != Snapshot.InitialInventory.InventoryId
		|| Snapshot.InitialView.InventoryRevision != Snapshot.InitialInventory.Revision
		|| Snapshot.View.InventoryId != Snapshot.Inventory.InventoryId
		|| Snapshot.View.InventoryRevision != Snapshot.Inventory.Revision)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGRestore"),
			LOCTEXT("Header", "The RPG snapshot has an invalid schema, identity, initial state, revision or journal bound."));
	}
	TArray<FKnightItemInstance> InitialItems;
	TSet<FGuid> InitialIds;
	TMap<FGuid, int32> InitialIndices;
	FKnightResult Result = Inventory->ValidateSnapshot(Snapshot.InitialInventory, InitialItems, InitialIds, InitialIndices);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	TArray<FKnightItemInstance> FinalItems;
	TSet<FGuid> FinalIds;
	TMap<FGuid, int32> FinalIndices;
	Result = Inventory->ValidateSnapshot(Snapshot.Inventory, FinalItems, FinalIds, FinalIndices);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (Snapshot.InitialInventory.Receipts.Num() > Snapshot.Inventory.Receipts.Num())
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestore"),
			LOCTEXT("InventoryPrefixCount", "The initial inventory journal cannot be longer than its final journal."));
	}
	for (int32 Index = 0; Index < Snapshot.InitialInventory.Receipts.Num(); ++Index)
	{
		if (!(Snapshot.InitialInventory.Receipts[Index] == Snapshot.Inventory.Receipts[Index]))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestore"),
				LOCTEXT("InventoryPrefix", "The complete initial inventory history must be an unchanged prefix of the final history."));
		}
	}
	FKnightRPGView Current = Snapshot.InitialView;
	TSet<FKnightEntityId> KnightIds;
	for (const FKnightRPGKnightView& Knight : Current.Knights)
	{
		if (!Knight.IsInitialized() || Knight.Revision != 1 || Knight.SkillPoints != Snapshot.InitialSkillPoints
			|| !Knight.Assignments.IsEmpty() || !Knight.PurchasedSkills.IsEmpty() || !Knight.StatBonuses.IsEmpty()
			|| !(Knight.RuneTuning == FKnightRuneTuning()) || KnightIds.Contains(Knight.KnightId))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGRestore"),
				LOCTEXT("InitialKnight", "Initial canonical RPG values must be distinct fresh Knights with the declared initial points."));
		}
		KnightIds.Add(Knight.KnightId);
	}
	SortView(Current);
	if (!(Current == Snapshot.InitialView))
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGRestore"),
			LOCTEXT("CanonicalOrder", "Snapshot values must use the canonical deterministic Knight and slot ordering."));
	}
	FKnightInventoryView CurrentInventory;
	CurrentInventory.InventoryId = Snapshot.InitialInventory.InventoryId;
	CurrentInventory.Capacity = Snapshot.InitialInventory.Capacity;
	CurrentInventory.Revision = Snapshot.InitialInventory.Revision;
	CurrentInventory.Items = MoveTemp(InitialItems);
	TSet<FGuid> UsedIds = MoveTemp(InitialIds);
	TSet<FGuid> RequestIds;
	TSet<FGuid> PickedIds;
	int32 InventoryReceiptIndex = Snapshot.InitialInventory.Receipts.Num();
	for (const FKnightRPGReceipt& Receipt : Snapshot.Receipts)
	{
		Result = Receipt.Command.ValidateShape();
		if (!Result.IsSuccess())
		{
			return Result;
		}
		if (!Receipt.HumanId.IsValid() || RequestIds.Contains(Receipt.Command.RequestId))
		{
			return FKnightResult::Failure(EKnightError::DuplicateId, TEXT("RPGRestore"),
				LOCTEXT("ReceiptIdentity", "Successful RPG receipts require valid human identities and unique exact request GUIDs."));
		}
		RequestIds.Add(Receipt.Command.RequestId);
		const bool bPickup = Receipt.Command.Operation == EKnightRPGOperation::Pickup;
		if ((bPickup && PickedIds.Contains(Receipt.Command.PickupId))
			|| (!bPickup && !(Receipt.PickedItem == FKnightItemInstance())))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestore"),
				LOCTEXT("PickupHistory", "Each pickup commits once, and unrelated commands cannot contain historical picked items."));
		}
		if (bPickup)
		{
			PickedIds.Add(Receipt.Command.PickupId);
		}
		FKnightRPGView Candidate;
		FKnightInventoryRequest DerivedRequest;
		FGuid Affected;
		Result = BuildCandidate(Current, CurrentInventory, Receipt.Command,
			bPickup ? &Receipt.PickedItem : nullptr, &Receipt.InventoryRequest.Mutations, Candidate, DerivedRequest, Affected);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		if (!(DerivedRequest == Receipt.InventoryRequest) || Affected != Receipt.AffectedInstanceId)
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestore"),
				LOCTEXT("ReceiptPayload", "The retained derived inventory mutation or affected identity disagrees with the actual command."));
		}
		if (!DerivedRequest.Mutations.IsEmpty())
		{
			if (!Snapshot.Inventory.Receipts.IsValidIndex(InventoryReceiptIndex)
				|| !(Snapshot.Inventory.Receipts[InventoryReceiptIndex].Request == DerivedRequest))
			{
				return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestore"),
					LOCTEXT("CostJournal", "Every RPG cost or pickup must match the next exact successful inventory receipt."));
			}
			TArray<FKnightItemInstance> CandidateItems;
			TSet<FGuid> CandidateIds;
			Result = Inventory->BuildCandidate(CurrentInventory.Items, UsedIds, CurrentInventory.Capacity,
				DerivedRequest, CandidateItems, CandidateIds);
			if (!Result.IsSuccess())
			{
				return Result;
			}
			CurrentInventory.Items = MoveTemp(CandidateItems);
			UsedIds = MoveTemp(CandidateIds);
			++CurrentInventory.Revision;
			++InventoryReceiptIndex;
		}
		Candidate.InventoryRevision = CurrentInventory.Revision;
		FKnightRPGKnightView* Knight = Candidate.Knights.FindByPredicate([&Receipt](const FKnightRPGKnightView& Entry)
			{
				return Entry.KnightId == Receipt.Command.ExpectedKnightId;
			});
		FKnightGASProjectionPlan Plan;
		Result = BuildProjection(*Knight, CurrentInventory.Items, Plan);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		if (!(*Knight == Receipt.CommittedKnight) || CurrentInventory.Revision != Receipt.CommittedInventoryRevision)
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestore"),
				LOCTEXT("KnightReceipt", "The retained Knight points, assignments, paid costs or derived stats disagree with journal reconstruction."));
		}
		Current = MoveTemp(Candidate);
	}
	if (InventoryReceiptIndex != Snapshot.Inventory.Receipts.Num() || CurrentInventory.Items != FinalItems
		|| CurrentInventory.Revision != Snapshot.Inventory.Revision || !(Current == Snapshot.View))
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestore"),
			LOCTEXT("FinalState", "The complete reconstructed RPG and inventory values disagree with the claimed final snapshot."));
	}
	OutView = MoveTemp(Current);
	return FKnightResult::Success();
}

FKnightResult UKnightRPGComponent::TryRestoreSnapshot(UKnightInventoryComponent* InInventory,
	const FKnightRPGSnapshot& InSnapshot, TConstArrayView<FKnightRPGKnightRegistration> InRegistrationView)
{
	const FKnightRPGSnapshot Snapshot = InSnapshot;
	TArray<FKnightRPGKnightRegistration> RestoreRegistrations;
	RestoreRegistrations.Append(InRegistrationView.GetData(), InRegistrationView.Num());
	const TConstArrayView<FKnightRPGKnightRegistration> InRegistrations(RestoreRegistrations);
	FKnightResult Result = ValidateAuthority();
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (IsInitialized() || bTransactionInProgress || !IsValid(InInventory) || InInventory->GetOwner() != GetOwner()
		|| InInventory->IsInitialized() || InInventory->IsReserved())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGRestore"),
			LOCTEXT("FreshOnly", "RPG reconstruction requires fresh unreserved coordinator and inventory domains."));
	}
	Result = ValidateRegistrations(InRegistrations, &Snapshot.View);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Result = ResolveSkills(Snapshot.View.SkillTreeId);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Inventory = InInventory;
	FKnightRPGView Candidate;
	Result = ValidateSnapshot(Snapshot, Candidate);
	if (!Result.IsSuccess())
	{
		Inventory = nullptr;
		return Result;
	}
	bTransactionInProgress = true;
	FKnightInventoryCapability InventoryCapability;
	TArray<FKnightMutationFence> Fences;
	TArray<FKnightProjectionCapability> Capabilities;
	TArray<UKnightAbilitySystemComponent*> ASCs;
	const TWeakObjectPtr<AActor> PublicationOwner(GetOwner());
	bool bCommitted = false;
	ON_SCOPE_EXIT
	{
		for (int32 Index = ASCs.Num() - 1; Index >= 0; --Index)
		{
			if (IsValid(ASCs[Index]) && !bCommitted && Capabilities[Index].IsValid())
			{
				ASCs[Index]->AbortSourceProjection(Fences[Index], Capabilities[Index]);
			}
			if (IsValid(ASCs[Index]) && Fences[Index].IsValid())
			{
				if (bCommitted && Capabilities[Index].IsValid())
				{
					ASCs[Index]->PublishSourceProjection(Fences[Index], Capabilities[Index], false, PublicationOwner.Get());
				}
				ASCs[Index]->EndMutationFence(Fences[Index]);
			}
		}
		if (IsValid(InInventory) && InventoryCapability.IsValid())
		{
			if (bCommitted)
			{
				InInventory->ResetPrepared();
			}
			else
			{
				InInventory->AbortPrepared(InventoryCapability);
			}
		}
		if (!bCommitted)
		{
			Inventory = nullptr;
		}
		bTransactionInProgress = false;
	};
	Result = Inventory->PrepareRestore(Snapshot.Inventory, InventoryCapability);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	for (const FKnightRPGKnightRegistration& Registration : InRegistrations)
	{
		UKnightAbilitySystemComponent* ASC = Registration.AbilitySystem.Get();
		ASCs.Add(ASC);
		Fences.AddDefaulted();
		Capabilities.AddDefaulted();
		FKnightRPGKnightView* Knight = Candidate.Knights.FindByPredicate([&Registration](const FKnightRPGKnightView& Entry)
			{
				return Entry.KnightId == Registration.KnightId;
			});
		FKnightGASProjectionPlan Plan;
		Result = BuildProjection(*Knight, Snapshot.Inventory.Items, Plan);
		if (Result.IsSuccess())
		{
			Result = ASC->BeginMutationFence(FGuid::NewGuid(), Fences.Last());
		}
		if (Result.IsSuccess())
		{
			Result = ASC->PrepareSourceProjection(Fences.Last(), Plan, 0, Capabilities.Last());
		}
		if (Result.IsSuccess())
		{
			Result = ASC->StageSourceProjection(Fences.Last(), Capabilities.Last());
		}
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	// Validate every participant after the last raw GAS staging callback, before the first commit.
	Result = ValidateAuthority();
	if (Result.IsSuccess())
	{
		Result = ValidateRegistrations(InRegistrations, &Snapshot.View, true);
	}
	FKnightInventoryView RevalidatedInventory;
	if (Result.IsSuccess())
	{
		Result = InInventory->GetPreparedView(InventoryCapability, RevalidatedInventory);
	}
	if (Result.IsSuccess() && (RevalidatedInventory.InventoryId != Snapshot.Inventory.InventoryId
		|| RevalidatedInventory.Revision != Snapshot.Inventory.Revision
		|| RevalidatedInventory.Capacity != Snapshot.Inventory.Capacity
		|| RevalidatedInventory.Items != Snapshot.Inventory.Items))
	{
		Result = FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRestoreStage"),
			LOCTEXT("ChangedPreparedInventory", "The exact prepared inventory changed during reconstruction staging."));
	}
	if (!Result.IsSuccess())
	{
		return Result;
	}
	for (int32 Index = 0; Index < ASCs.Num(); ++Index)
	{
		Result = ASCs[Index]->ValidateStagedSourceProjection(Fences[Index], Capabilities[Index]);
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	for (int32 Index = 0; Index < ASCs.Num(); ++Index)
	{
		Result = ASCs[Index]->CommitSourceProjection(Fences[Index], Capabilities[Index]);
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	Result = Inventory->CommitPrepared(InventoryCapability);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	InitialSkillPoints = Snapshot.InitialSkillPoints;
	InitialInventory = Snapshot.InitialInventory;
	InitialView = Snapshot.InitialView;
	PublishedView = MoveTemp(Candidate);
	Receipts = Snapshot.Receipts;
	for (int32 Index = 0; Index < Receipts.Num(); ++Index)
	{
		ReceiptIndices.Add(Receipts[Index].Command.RequestId, Index);
	}
	for (const FKnightRPGKnightRegistration& Registration : InRegistrations)
	{
		Registrations.Add(Registration.KnightId, Registration);
	}
	for (const FKnightRPGKnightView& Knight : PublishedView.Knights)
	{
		for (const FKnightEquipmentAssignment& Assignment : Knight.Assignments)
		{
			Inventory->AssignedInstanceIds.Add(Assignment.ItemInstanceId);
		}
	}
	BindInventory(InInventory);
	bCommitted = true;
	RefreshReplicatedState();
	const auto CanPublish = [this, InRegistrations, &Snapshot]()
	{
		return ValidateAuthority().IsSuccess()
			&& ValidateRegistrations(InRegistrations, &Snapshot.View, true, false).IsSuccess();
	};
	for (int32 Index = 0; Index < ASCs.Num(); ++Index)
	{
		ASCs[Index]->PublishSourceProjection(Fences[Index], Capabilities[Index], true, PublicationOwner.Get());
		if (!CanPublish())
		{
			return FKnightResult::Success();
		}
	}
	InInventory->PublishPrepared(InventoryCapability, false);
	if (!CanPublish())
	{
		return FKnightResult::Success();
	}
	OnSnapshotChanged.Broadcast();
	return FKnightResult::Success();
}

void UKnightRPGComponent::RefreshReplicatedState()
{
	ReplicatedState.Owner = this;
	ReplicatedState.Records.Reset();
	FKnightRPGRecord& Control = ReplicatedState.Records.AddDefaulted_GetRef();
	Control.bIsControl = true;
	Control.Revision = PublishedView.Revision;
	Control.InventoryId = PublishedView.InventoryId;
	Control.InventoryRevision = PublishedView.InventoryRevision;
	Control.SkillTreeId = PublishedView.SkillTreeId;
	ReplicatedState.MarkItemDirty(Control);
	for (const FKnightRPGKnightView& Knight : PublishedView.Knights)
	{
		FKnightRPGRecord& Record = ReplicatedState.Records.AddDefaulted_GetRef();
		Record.Revision = PublishedView.Revision;
		Record.Knight = Knight;
		ReplicatedState.MarkItemDirty(Record);
	}
	ReplicatedState.MarkArrayDirty();
	GetOwner()->ForceNetUpdate();
}

void UKnightRPGComponent::HandleReplicatedStateReady()
{
	if (bEndingPlay || Inventory == nullptr || GetOwner() == nullptr || GetOwner()->HasAuthority())
	{
		return;
	}
	const FKnightRPGRecord* Control = nullptr;
	for (const FKnightRPGRecord& Record : ReplicatedState.Records)
	{
		if (Record.bIsControl)
		{
			if (Control != nullptr)
			{
				return;
			}
			Control = &Record;
		}
	}
	const FKnightInventoryView InventoryView = Inventory->GetView();
	if (Control == nullptr || Control->Revision <= PublishedView.Revision || Control->Revision < 1
		|| Control->InventoryId != InventoryView.InventoryId || Control->InventoryRevision != InventoryView.Revision
		|| !KnightIdentity::IsValidDefinitionId(Control->SkillTreeId)
		|| Control->SkillTreeId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightSkillTree")))
	{
		return;
	}
	FKnightRPGView Candidate;
	Candidate.Revision = Control->Revision;
	Candidate.InventoryId = Control->InventoryId;
	Candidate.InventoryRevision = Control->InventoryRevision;
	Candidate.SkillTreeId = Control->SkillTreeId;
	TSet<FKnightEntityId> KnightIds;
	TSet<FGuid> AssignedIds;
	for (const FKnightRPGRecord& Record : ReplicatedState.Records)
	{
		if (Record.bIsControl || Record.Revision != Candidate.Revision)
		{
			continue;
		}
		if (!Record.Knight.IsInitialized() || KnightIds.Contains(Record.Knight.KnightId)
			|| Record.Knight.Assignments.Num() > 2 || Record.Knight.PurchasedSkills.Num() > KnightRPG::MaximumSkills
			|| !Record.Knight.StatBonuses.Validate().IsSuccess() || !Record.Knight.RuneTuning.Validate().IsSuccess())
		{
			return;
		}
		for (const FKnightEquipmentAssignment& Assignment : Record.Knight.Assignments)
		{
			if (AssignedIds.Contains(Assignment.ItemInstanceId) || !InventoryView.Items.ContainsByPredicate(
				[&Assignment](const FKnightItemInstance& Item)
				{
					return Item.InstanceId == Assignment.ItemInstanceId && Item.DefinitionId == Assignment.DefinitionId
						&& Item.Count == 1;
				}))
			{
				return;
			}
			AssignedIds.Add(Assignment.ItemInstanceId);
		}
		KnightIds.Add(Record.Knight.KnightId);
		Candidate.Knights.Add(Record.Knight);
	}
	if (Candidate.Knights.Num() != 3)
	{
		return;
	}
	SortView(Candidate);
	PublishedView = MoveTemp(Candidate);
	OnSnapshotChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE
