#include "Inventory/KnightInventoryComponent.h"

#include "Assets/KnightAssetManager.h"
#include "Definitions/KnightItemDefinition.h"
#include "Types/KnightStableIds.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "KnightInventoryComponent"

bool FKnightInventoryCapability::IsValid() const
{
	return Owner.IsValid() && Token.IsValid();
}

bool FKnightInventoryArray::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
{
	return FFastArraySerializer::FastArrayDeltaSerialize<FKnightInventoryRecord, FKnightInventoryArray>(
		Records, DeltaParams, *this);
}

void FKnightInventoryArray::PostReplicatedReceive(
	const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (UKnightInventoryComponent* Component = Owner.Get())
	{
		Component->HandleReplicatedStateReady();
	}
}

UKnightInventoryComponent::UKnightInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UKnightInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKnightInventoryComponent, ReplicatedState);
}

FKnightInventoryView UKnightInventoryComponent::GetView() const
{
	return PublishedView;
}

bool UKnightInventoryComponent::IsInitialized() const
{
	return PublishedView.IsInitialized();
}

FKnightResult UKnightInventoryComponent::InitializeInventory(int32 InCapacity)
{
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsReserved())
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("InventoryInitialize"),
			LOCTEXT("InventoryReserved", "The inventory is reserved by an atomic domain transaction."));
	}
	if (IsInitialized())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("InventoryInitialize"),
			LOCTEXT("AlreadyInitialized", "An inventory cannot be reinitialized or reset by replacing its identity."));
	}
	if (InCapacity < 1 || InCapacity > KnightItems::MaximumInventoryCapacity)
	{
		return FKnightResult::Failure(EKnightError::CapacityExceeded, TEXT("InventoryInitialize"),
			LOCTEXT("InvalidCapacity", "Inventory capacity is outside the supported finite range."));
	}
	PublishedView.InventoryId = FGuid::NewGuid();
	PublishedView.Capacity = InCapacity;
	PublishedView.Revision = 1;
	RefreshReplicatedState();
	OnSnapshotChanged.Broadcast();
	return FKnightResult::Success();
}

FKnightResult UKnightInventoryComponent::InitializeFixtureCatalog(TConstArrayView<UKnightItemDefinition*> Definitions)
{
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsInitialized() || bHasFixtureCatalog || IsReserved())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("InventoryFixtureCatalog"),
			LOCTEXT("FixtureCatalogLocked", "A fixture catalog may be configured only once before inventory initialization."));
	}
	if (Definitions.IsEmpty() || Definitions.Num() > KnightItems::MaximumInventoryCapacity)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("InventoryFixtureCatalog"),
			LOCTEXT("FixtureCatalogSize", "A fixture catalog requires a finite nonempty set of actual item definitions."));
	}
	TSet<FPrimaryAssetId> Ids;
	for (const UKnightItemDefinition* Definition : Definitions)
	{
		if (!IsValid(Definition))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("InventoryFixtureCatalog"),
				LOCTEXT("NullFixtureDefinition", "A fixture catalog cannot contain a null or destroyed definition."));
		}
		const FKnightResult Validation = Definition->ValidateItem();
		if (!Validation.IsSuccess())
		{
			return Validation;
		}
		if (Ids.Contains(Definition->GetPrimaryAssetId()))
		{
			return FKnightResult::Failure(EKnightError::DuplicateId, TEXT("InventoryFixtureCatalog"),
				LOCTEXT("DuplicateFixtureDefinition", "A fixture catalog cannot resolve duplicated definition IDs."));
		}
		Ids.Add(Definition->GetPrimaryAssetId());
	}
	TMap<FPrimaryAssetId, TObjectPtr<UKnightItemDefinition>> Copies;
	for (UKnightItemDefinition* Definition : Definitions)
	{
		UKnightItemDefinition* Copy = DuplicateObject<UKnightItemDefinition>(Definition, this);
		if (!IsValid(Copy) || !Copy->ValidateItem().IsSuccess()
			|| Copy->GetPrimaryAssetId() != Definition->GetPrimaryAssetId())
		{
			return FKnightResult::Failure(EKnightError::LoadFailed, TEXT("InventoryFixtureCatalog"),
				LOCTEXT("FixtureCopyFailed", "A validated item definition could not be copied into the immutable fixture catalog."));
		}
		Copy->SetFlags(RF_Transient);
		Copies.Add(Copy->GetPrimaryAssetId(), Copy);
	}
	FixtureDefinitions = MoveTemp(Copies);
	bHasFixtureCatalog = true;
	return FKnightResult::Success();
}

FKnightInventoryTransactionResult UKnightInventoryComponent::TryApplyTransaction(const FKnightInventoryRequest& Request)
{
	FKnightInventoryTransactionResult Outcome;
	Outcome.Result = ValidateAuthority();
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	if (IsReserved())
	{
		Outcome.Result = FKnightResult::Failure(EKnightError::Conflict, TEXT("InventoryTransaction"),
			LOCTEXT("TransactionReserved", "A coordinated transaction currently reserves this inventory."));
		return Outcome;
	}
	if (!IsInitialized())
	{
		Outcome.Result = FKnightResult::Failure(EKnightError::InvalidState, TEXT("InventoryTransaction"),
			LOCTEXT("InventoryNotInitialized", "Inventory transactions require an initialized authoritative inventory."));
		return Outcome;
	}
	if (const int32* ReceiptIndex = ReceiptIndices.Find(Request.RequestId))
	{
		const FKnightInventoryReceipt& Receipt = Receipts[*ReceiptIndex];
		if (!(Receipt.Request == Request))
		{
			Outcome.Result = FKnightResult::Failure(EKnightError::Conflict, TEXT("InventoryTransaction"),
				LOCTEXT("ChangedReplayPayload", "This request GUID already committed a different exact transaction payload."));
			return Outcome;
		}
		Outcome.Result = FKnightResult::Success();
		Outcome.CommittedRevision = Receipt.CommittedRevision;
		Outcome.bWasReplay = true;
		return Outcome;
	}
	Outcome.Result = Request.ValidateShape();
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	if (Request.ExpectedRevision != PublishedView.Revision)
	{
		Outcome.Result = FKnightResult::Failure(EKnightError::InvalidRevision, TEXT("InventoryTransaction"),
			LOCTEXT("StaleRevision", "Inventory changed before this transaction; read its current revision before retrying."));
		return Outcome;
	}
	if (Receipts.Num() >= KnightItems::MaximumTransactionReceipts)
	{
		Outcome.Result = FKnightResult::Failure(EKnightError::CapacityExceeded, TEXT("InventoryReceiptJournal"),
			LOCTEXT("ReceiptJournalFull", "The bounded transaction journal is full; writes cannot discard replay protection."));
		return Outcome;
	}
	TArray<FKnightItemInstance> CandidateItems;
	TSet<FGuid> CandidateIds;
	Outcome.Result = BuildCandidate(PublishedView.Items, UsedInstanceIds, PublishedView.Capacity,
		Request, CandidateItems, CandidateIds);
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	FKnightInventoryReceipt Receipt;
	Receipt.Request = Request;
	Receipt.CommittedRevision = PublishedView.Revision + 1;
	Outcome.CommittedRevision = Receipt.CommittedRevision;
	PublishedView.Items = MoveTemp(CandidateItems);
	UsedInstanceIds = MoveTemp(CandidateIds);
	PublishedView.Revision = Receipt.CommittedRevision;
	ReceiptIndices.Add(Receipt.Request.RequestId, Receipts.Num());
	Receipts.Add(MoveTemp(Receipt));
	RefreshReplicatedState();
	// All containers, receipts and revision are committed before subscribers can submit another native command.
	OnSnapshotChanged.Broadcast();
	return Outcome;
}

FKnightResult UKnightInventoryComponent::CaptureSnapshot(FKnightInventorySnapshot& OutSnapshot) const
{
	OutSnapshot = FKnightInventorySnapshot();
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (!IsInitialized())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("InventorySnapshot"),
			LOCTEXT("SnapshotNotInitialized", "Only an initialized inventory can capture a persistence fragment."));
	}
	OutSnapshot.InventoryId = PublishedView.InventoryId;
	OutSnapshot.Capacity = PublishedView.Capacity;
	OutSnapshot.Revision = PublishedView.Revision;
	OutSnapshot.Items = PublishedView.Items;
	OutSnapshot.Receipts = Receipts;
	return FKnightResult::Success();
}

FKnightResult UKnightInventoryComponent::ValidateSnapshot(const FKnightInventorySnapshot& Snapshot,
	TArray<FKnightItemInstance>& OutItems, TSet<FGuid>& OutIds, TMap<FGuid, int32>& OutReceipts) const
{
	if (Snapshot.SchemaVersion != KnightItems::InventorySnapshotVersion || !Snapshot.InventoryId.IsValid()
		|| Snapshot.Capacity < 1 || Snapshot.Capacity > KnightItems::MaximumInventoryCapacity
		|| Snapshot.Receipts.Num() > KnightItems::MaximumTransactionReceipts)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("InventoryRestore"),
			LOCTEXT("InvalidSnapshotHeader", "The inventory snapshot has an invalid version, identity, capacity or journal bound."));
	}
	if (Snapshot.Revision != static_cast<int64>(Snapshot.Receipts.Num()) + 1)
	{
		return FKnightResult::Failure(EKnightError::InvalidRevision, TEXT("InventoryRestore"),
			LOCTEXT("InvalidSnapshotRevision", "The claimed inventory revision does not match its complete successful journal."));
	}
	const FKnightResult ItemsResult = ValidateItems(Snapshot.Items, Snapshot.Capacity);
	if (!ItemsResult.IsSuccess())
	{
		return ItemsResult;
	}
	TArray<FKnightItemInstance> ReconstructedItems;
	TSet<FGuid> ReconstructedIds;
	TMap<FGuid, int32> ReconstructedReceipts;
	int64 ReconstructedRevision = 1;
	for (int32 Index = 0; Index < Snapshot.Receipts.Num(); ++Index)
	{
		const FKnightInventoryReceipt& Receipt = Snapshot.Receipts[Index];
		const FKnightResult Shape = Receipt.Request.ValidateShape();
		if (!Shape.IsSuccess())
		{
			return Shape;
		}
		if (ReconstructedReceipts.Contains(Receipt.Request.RequestId))
		{
			return FKnightResult::Failure(EKnightError::DuplicateId, TEXT("InventoryRestore"),
				LOCTEXT("DuplicateSnapshotReceipt", "The successful journal contains a duplicated request GUID."));
		}
		if (Receipt.Request.ExpectedRevision != ReconstructedRevision
			|| Receipt.CommittedRevision != ReconstructedRevision + 1)
		{
			return FKnightResult::Failure(EKnightError::InvalidRevision, TEXT("InventoryRestore"),
				LOCTEXT("InvalidReceiptRevision", "Successful receipts must form one ordered, contiguous revision history."));
		}
		TArray<FKnightItemInstance> CandidateItems;
		TSet<FGuid> CandidateIds;
		const FKnightResult CandidateResult = BuildCandidate(ReconstructedItems, ReconstructedIds,
			Snapshot.Capacity, Receipt.Request, CandidateItems, CandidateIds);
		if (!CandidateResult.IsSuccess())
		{
			return CandidateResult;
		}
		ReconstructedItems = MoveTemp(CandidateItems);
		ReconstructedIds = MoveTemp(CandidateIds);
		ReconstructedReceipts.Add(Receipt.Request.RequestId, Index);
		++ReconstructedRevision;
	}
	TArray<FKnightItemInstance> ClaimedItems = Snapshot.Items;
	SortItems(ClaimedItems);
	if (ReconstructedItems != ClaimedItems)
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("InventoryRestore"),
			LOCTEXT("SnapshotStateMismatch", "The claimed final item identities and counts disagree with the actual transaction history."));
	}
	OutItems = MoveTemp(ReconstructedItems);
	OutReceipts = MoveTemp(ReconstructedReceipts);
	OutIds = MoveTemp(ReconstructedIds);
	return FKnightResult::Success();
}

FKnightResult UKnightInventoryComponent::TryRestoreSnapshot(const FKnightInventorySnapshot& Snapshot)
{
	FKnightInventoryCapability Capability;
	FKnightResult Result = PrepareRestore(Snapshot, Capability);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Result = CommitPrepared(Capability);
	if (!Result.IsSuccess())
	{
		AbortPrepared(Capability);
		return Result;
	}
	return PublishPrepared(Capability);
}

bool UKnightInventoryComponent::IsReserved() const
{
	return PreparedToken.IsValid();
}

FKnightResult UKnightInventoryComponent::ValidateCapability(const FKnightInventoryCapability& Capability) const
{
	if (Capability.Owner.Get() != this || !Capability.Token.IsValid() || Capability.Token != PreparedToken)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("InventoryCapability"),
			LOCTEXT("InvalidCapability", "This capability does not own the inventory's current reservation."));
	}
	return FKnightResult::Success();
}

FKnightResult UKnightInventoryComponent::ReserveRead(const FGuid& InventoryId, int64 ExpectedRevision,
	FKnightInventoryCapability& OutCapability)
{
	OutCapability = FKnightInventoryCapability();
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsReserved())
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("InventoryReserve"),
			LOCTEXT("ReadReserved", "Another command already reserves this inventory."));
	}
	if (!IsInitialized() || InventoryId != PublishedView.InventoryId)
	{
		return FKnightResult::Failure(EKnightError::InvalidId, TEXT("InventoryReserve"),
			LOCTEXT("ReadWrongInventory", "The read reservation requires the current initialized inventory identity."));
	}
	if (ExpectedRevision != PublishedView.Revision)
	{
		return FKnightResult::Failure(EKnightError::InvalidRevision, TEXT("InventoryReserve"),
			LOCTEXT("ReadStaleRevision", "The inventory revision changed before this command was prepared."));
	}
	PreparedToken = FGuid::NewGuid();
	PreparedView = PublishedView;
	PreparedIds = UsedInstanceIds;
	OutCapability.Owner = this;
	OutCapability.Token = PreparedToken;
	return FKnightResult::Success();
}

FKnightResult UKnightInventoryComponent::PrepareTransaction(const FKnightInventoryRequest& Request,
	FKnightInventoryCapability& OutCapability)
{
	OutCapability = FKnightInventoryCapability();
	if (IsReserved())
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("InventoryPrepare"),
			LOCTEXT("PrepareReserved", "Another command already reserves this inventory."));
	}
	const FKnightResult Shape = Request.ValidateShape();
	if (!Shape.IsSuccess())
	{
		return Shape;
	}
	if (ReceiptIndices.Contains(Request.RequestId))
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("InventoryPrepare"),
			LOCTEXT("PrepareAlreadyCommitted", "A prepared write must be new; the coordinator authenticates successful replays."));
	}
	if (Receipts.Num() >= KnightItems::MaximumTransactionReceipts)
	{
		return FKnightResult::Failure(EKnightError::CapacityExceeded, TEXT("InventoryPrepare"),
			LOCTEXT("PreparedJournalFull", "The complete successful journal reached its finite receipt limit."));
	}
	FKnightResult Result = ReserveRead(PublishedView.InventoryId, Request.ExpectedRevision, OutCapability);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Result = BuildCandidate(PublishedView.Items, UsedInstanceIds, PublishedView.Capacity,
		Request, PreparedView.Items, PreparedIds);
	if (!Result.IsSuccess())
	{
		ResetPrepared();
		OutCapability = FKnightInventoryCapability();
		return Result;
	}
	PreparedRequest = Request;
	PreparedView.Revision = PublishedView.Revision + 1;
	bPreparedWrite = true;
	return FKnightResult::Success();
}

FKnightResult UKnightInventoryComponent::PrepareRestore(const FKnightInventorySnapshot& Snapshot,
	FKnightInventoryCapability& OutCapability)
{
	OutCapability = FKnightInventoryCapability();
	const FKnightResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsReserved())
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("InventoryRestore"),
			LOCTEXT("RestoreReserved", "A reserved inventory cannot prepare reconstruction."));
	}
	if (IsInitialized())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("InventoryRestore"),
			LOCTEXT("RestoreFreshOnly", "Reconstruction requires a fresh inventory and never overwrites live state."));
	}
	TArray<FKnightItemInstance> Items;
	TSet<FGuid> Ids;
	TMap<FGuid, int32> Indices;
	const FKnightResult Validation = ValidateSnapshot(Snapshot, Items, Ids, Indices);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	PreparedToken = FGuid::NewGuid();
	PreparedView.InventoryId = Snapshot.InventoryId;
	PreparedView.Capacity = Snapshot.Capacity;
	PreparedView.Revision = Snapshot.Revision;
	PreparedView.Items = MoveTemp(Items);
	PreparedIds = MoveTemp(Ids);
	PreparedReceiptIndices = MoveTemp(Indices);
	PreparedSnapshot = Snapshot;
	bPreparedRestore = true;
	bPreparedWrite = true;
	OutCapability.Owner = this;
	OutCapability.Token = PreparedToken;
	return FKnightResult::Success();
}

FKnightResult UKnightInventoryComponent::GetPreparedView(const FKnightInventoryCapability& Capability,
	FKnightInventoryView& OutView) const
{
	OutView = FKnightInventoryView();
	const FKnightResult Validation = ValidateCapability(Capability);
	if (Validation.IsSuccess())
	{
		OutView = PreparedView;
	}
	return Validation;
}

FKnightResult UKnightInventoryComponent::CommitPrepared(const FKnightInventoryCapability& Capability)
{
	const FKnightResult Validation = ValidateCapability(Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (bPreparedCommitted)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("InventoryCommit"),
			LOCTEXT("AlreadyPreparedCommitted", "This reservation has already committed its candidate."));
	}
	if (bPreparedWrite)
	{
		PublishedView = PreparedView;
		UsedInstanceIds = PreparedIds;
		if (bPreparedRestore)
		{
			Receipts = PreparedSnapshot.Receipts;
			ReceiptIndices = PreparedReceiptIndices;
		}
		else
		{
			FKnightInventoryReceipt Receipt;
			Receipt.Request = PreparedRequest;
			Receipt.CommittedRevision = PublishedView.Revision;
			ReceiptIndices.Add(Receipt.Request.RequestId, Receipts.Num());
			Receipts.Add(MoveTemp(Receipt));
		}
		RefreshReplicatedState();
	}
	bPreparedCommitted = true;
	return FKnightResult::Success();
}

FKnightResult UKnightInventoryComponent::AbortPrepared(const FKnightInventoryCapability& Capability)
{
	const FKnightResult Validation = ValidateCapability(Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (bPreparedCommitted)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("InventoryAbort"),
			LOCTEXT("CannotAbortCommitted", "A committed domain transaction cannot be rolled back with a fresh snapshot."));
	}
	ResetPrepared();
	return FKnightResult::Success();
}

FKnightResult UKnightInventoryComponent::PublishPrepared(const FKnightInventoryCapability& Capability,
	bool bReleaseReservation)
{
	const FKnightResult Validation = ValidateCapability(Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (!bPreparedCommitted)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("InventoryPublish"),
			LOCTEXT("CannotPublishUncommitted", "A prepared candidate must commit before publication."));
	}
	if (bPreparedPublishing || bPreparedPublished)
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("InventoryPublish"),
			LOCTEXT("AlreadyPublishing", "This exact committed reservation can publish only once."));
	}
	const FGuid PublicationToken = PreparedToken;
	bPreparedPublishing = true;
	bPreparedPublished = true;
	if (bPreparedWrite)
	{
		OnSnapshotChanged.Broadcast();
	}
	if (PreparedToken == PublicationToken && bReleaseReservation)
	{
		ResetPrepared();
	}
	else if (PreparedToken == PublicationToken)
	{
		bPreparedPublishing = false;
	}
	return FKnightResult::Success();
}

void UKnightInventoryComponent::ResetPrepared()
{
	PreparedToken.Invalidate();
	PreparedView = FKnightInventoryView();
	PreparedIds.Reset();
	PreparedRequest = FKnightInventoryRequest();
	PreparedSnapshot = FKnightInventorySnapshot();
	PreparedReceiptIndices.Reset();
	bPreparedWrite = false;
	bPreparedRestore = false;
	bPreparedCommitted = false;
	bPreparedPublishing = false;
	bPreparedPublished = false;
}

void UKnightInventoryComponent::OnRegister()
{
	Super::OnRegister();
	ReplicatedState.Owner = this;
}

void UKnightInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	ResetPrepared();
	ReplicatedState.Owner.Reset();
	Super::EndPlay(EndPlayReason);
}

FKnightResult UKnightInventoryComponent::ValidateAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	if (!IsInGameThread() || bEndingPlay || !IsValid(OwnerActor) || OwnerActor->IsActorBeingDestroyed()
		|| World == nullptr || World->bIsTearingDown)
	{
		return FKnightResult::Failure(EKnightError::ShuttingDown, TEXT("InventoryAuthority"),
			LOCTEXT("InventoryLifetimeEnded", "The inventory owner or its world is unavailable for this command."));
	}
	if (!OwnerActor->HasAuthority() || World->GetNetMode() == NM_Client)
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("InventoryAuthority"),
			LOCTEXT("InventoryServerOnly", "Only the authoritative campaign server may mutate or persist shared inventory."));
	}
	return FKnightResult::Success();
}

FKnightResult UKnightInventoryComponent::ResolveItemDefinition(const FPrimaryAssetId& DefinitionId,
	const UKnightItemDefinition*& OutDefinition) const
{
	OutDefinition = nullptr;
	if (!KnightIdentity::IsValidDefinitionId(DefinitionId)
		|| DefinitionId.PrimaryAssetType != FPrimaryAssetType(TEXT("KnightItem")))
	{
		return FKnightResult::Failure(EKnightError::InvalidId, TEXT("InventoryDefinition"),
			LOCTEXT("InvalidInventoryDefinition", "Inventory requires a valid stable KnightItem definition ID."));
	}
	if (bHasFixtureCatalog)
	{
		const TObjectPtr<UKnightItemDefinition>* Definition = FixtureDefinitions.Find(DefinitionId);
		if (Definition == nullptr || !IsValid(Definition->Get()))
		{
			return FKnightResult::Failure(EKnightError::NotFound, FName(*DefinitionId.ToString()),
				LOCTEXT("FixtureDefinitionMissing", "No validated immutable fixture definition matches this item identity."));
		}
		OutDefinition = Definition->Get();
	}
	else
	{
		UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
		if (Manager == nullptr)
		{
			return FKnightResult::Failure(EKnightError::LoadFailed, TEXT("InventoryDefinition"),
				LOCTEXT("ItemAssetManagerUnavailable", "The configured Knight Asset Manager is unavailable."));
		}
		UKnightDefinition* Definition = nullptr;
		const FKnightResult Resolution = Manager->ResolveDefinition(DefinitionId,
			UKnightItemDefinition::StaticClass(), Definition);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		OutDefinition = Cast<UKnightItemDefinition>(Definition);
	}
	if (OutDefinition == nullptr || OutDefinition->GetPrimaryAssetId() != DefinitionId)
	{
		OutDefinition = nullptr;
		return FKnightResult::Failure(EKnightError::TypeMismatch, FName(*DefinitionId.ToString()),
			LOCTEXT("WrongResolvedDefinition", "The resolved asset is not the exact requested typed item definition."));
	}
	const FKnightResult Validation = OutDefinition->ValidateItem();
	if (!Validation.IsSuccess())
	{
		OutDefinition = nullptr;
	}
	return Validation;
}

FKnightResult UKnightInventoryComponent::BuildCandidate(const TArray<FKnightItemInstance>& ExistingItems,
	const TSet<FGuid>& ExistingInstanceIds, int32 InCapacity, const FKnightInventoryRequest& Request,
	TArray<FKnightItemInstance>& OutItems, TSet<FGuid>& OutInstanceIds) const
{
	OutItems = ExistingItems;
	OutInstanceIds = ExistingInstanceIds;
	for (const FKnightInventoryMutation& Mutation : Request.Mutations)
	{
		const UKnightItemDefinition* Definition = nullptr;
		const FKnightResult Resolution = ResolveItemDefinition(Mutation.Item.DefinitionId, Definition);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		const int32 ExistingIndex = OutItems.IndexOfByPredicate([&Mutation](const FKnightItemInstance& Item)
			{
				return Item.InstanceId == Mutation.Item.InstanceId;
			});
		if (ExistingIndex != INDEX_NONE && OutItems[ExistingIndex].DefinitionId != Mutation.Item.DefinitionId)
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("InventoryInstance"),
				LOCTEXT("InstanceDefinitionMismatch", "An instance GUID cannot change its stable definition identity."));
		}
		if (Mutation.Operation == EKnightInventoryOperation::Add)
		{
			if (ExistingIndex == INDEX_NONE)
			{
				if (OutInstanceIds.Contains(Mutation.Item.InstanceId))
				{
					return FKnightResult::Failure(EKnightError::DuplicateId, TEXT("InventoryInstance"),
						LOCTEXT("RetiredInstanceId", "A removed instance GUID cannot be reused for a newly created item."));
				}
				OutItems.Add(Mutation.Item);
				OutInstanceIds.Add(Mutation.Item.InstanceId);
			}
			else
			{
				const int64 CombinedCount = static_cast<int64>(OutItems[ExistingIndex].Count) + Mutation.Item.Count;
				if (CombinedCount > Definition->GetMaximumStack())
				{
					return FKnightResult::Failure(EKnightError::CapacityExceeded, TEXT("InventoryStack"),
						LOCTEXT("StackExceeded", "The transaction would exceed the authored maximum stack."));
				}
				OutItems[ExistingIndex].Count = static_cast<int32>(CombinedCount);
			}
		}
		else
		{
			if (AssignedInstanceIds.Contains(Mutation.Item.InstanceId))
			{
				return FKnightResult::Failure(EKnightError::NotOwned, TEXT("InventoryAssignment"),
					LOCTEXT("AssignedRemoval", "An assigned equipment instance must be unequipped before removal."));
			}
			if (ExistingIndex == INDEX_NONE)
			{
				return FKnightResult::Failure(EKnightError::NotFound, TEXT("InventoryInstance"),
					LOCTEXT("MissingRemovalInstance", "The exact item instance to remove does not exist in this inventory."));
			}
			if (Mutation.Item.Count > OutItems[ExistingIndex].Count)
			{
				return FKnightResult::Failure(EKnightError::InsufficientResources, TEXT("InventoryCount"),
					LOCTEXT("RemovalCountExceeded", "The requested removal exceeds the current item count."));
			}
			OutItems[ExistingIndex].Count -= Mutation.Item.Count;
			if (OutItems[ExistingIndex].Count == 0)
			{
				OutItems.RemoveAt(ExistingIndex);
			}
		}
	}
	const FKnightResult Validation = ValidateItems(OutItems, InCapacity);
	if (Validation.IsSuccess())
	{
		SortItems(OutItems);
	}
	return Validation;
}

FKnightResult UKnightInventoryComponent::ValidateItems(const TArray<FKnightItemInstance>& Items, int32 InCapacity) const
{
	if (InCapacity < 1 || InCapacity > KnightItems::MaximumInventoryCapacity || Items.Num() > InCapacity)
	{
		return FKnightResult::Failure(EKnightError::CapacityExceeded, TEXT("InventoryCapacity"),
			LOCTEXT("InventoryCapacityExceeded", "The complete candidate inventory exceeds its finite instance capacity."));
	}
	TSet<FGuid> Ids;
	for (const FKnightItemInstance& Item : Items)
	{
		const FKnightResult Identity = Item.ValidateIdentityAndCount();
		if (!Identity.IsSuccess())
		{
			return Identity;
		}
		if (Ids.Contains(Item.InstanceId))
		{
			return FKnightResult::Failure(EKnightError::DuplicateId, TEXT("InventoryInstance"),
				LOCTEXT("DuplicateInventoryInstance", "Inventory cannot contain two entries with the same persistent instance GUID."));
		}
		Ids.Add(Item.InstanceId);
		const UKnightItemDefinition* Definition = nullptr;
		const FKnightResult Resolution = ResolveItemDefinition(Item.DefinitionId, Definition);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		if (Item.Count > Definition->GetMaximumStack())
		{
			return FKnightResult::Failure(EKnightError::CapacityExceeded, TEXT("InventoryStack"),
				LOCTEXT("InvalidCandidateStack", "An item count exceeds its validated definition's maximum stack."));
		}
	}
	return FKnightResult::Success();
}

void UKnightInventoryComponent::RefreshReplicatedState()
{
	ReplicatedState.Owner = this;
	for (int32 Index = ReplicatedState.Records.Num() - 1; Index >= 0; --Index)
	{
		const FKnightInventoryRecord& Record = ReplicatedState.Records[Index];
		if (!Record.bIsControl && !PublishedView.Items.ContainsByPredicate([&Record](const FKnightItemInstance& Item)
			{
				return Item.InstanceId == Record.Item.InstanceId;
			}))
		{
			ReplicatedState.Records.RemoveAt(Index);
			ReplicatedState.MarkArrayDirty();
		}
	}
	int32 ControlIndex = ReplicatedState.Records.IndexOfByPredicate([](const FKnightInventoryRecord& Record)
		{
			return Record.bIsControl;
		});
	if (ControlIndex == INDEX_NONE)
	{
		ControlIndex = ReplicatedState.Records.AddDefaulted();
	}
	FKnightInventoryRecord& Control = ReplicatedState.Records[ControlIndex];
	Control.bIsControl = true;
	Control.InventoryId = PublishedView.InventoryId;
	Control.Capacity = PublishedView.Capacity;
	Control.ItemCount = PublishedView.Items.Num();
	Control.Revision = PublishedView.Revision;
	ReplicatedState.MarkItemDirty(Control);
	for (const FKnightItemInstance& Item : PublishedView.Items)
	{
		int32 RecordIndex = ReplicatedState.Records.IndexOfByPredicate([&Item](const FKnightInventoryRecord& Record)
			{
				return !Record.bIsControl && Record.Item.InstanceId == Item.InstanceId;
			});
		if (RecordIndex == INDEX_NONE)
		{
			RecordIndex = ReplicatedState.Records.AddDefaulted();
		}
		FKnightInventoryRecord& Record = ReplicatedState.Records[RecordIndex];
		Record.Item = Item;
		Record.Revision = PublishedView.Revision;
		ReplicatedState.MarkItemDirty(Record);
	}
	GetOwner()->ForceNetUpdate();
}

void UKnightInventoryComponent::HandleReplicatedStateReady()
{
	if (bEndingPlay)
	{
		return;
	}
	const FKnightInventoryRecord* Control = nullptr;
	for (const FKnightInventoryRecord& Record : ReplicatedState.Records)
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
	if (Control == nullptr || !Control->InventoryId.IsValid() || Control->Revision < 1
		|| Control->Revision <= PublishedView.Revision || Control->Capacity < 1
		|| Control->Capacity > KnightItems::MaximumInventoryCapacity
		|| Control->ItemCount < 0 || Control->ItemCount > Control->Capacity
		|| (IsInitialized() && Control->InventoryId != PublishedView.InventoryId))
	{
		return;
	}
	FKnightInventoryView Candidate;
	Candidate.InventoryId = Control->InventoryId;
	Candidate.Revision = Control->Revision;
	Candidate.Capacity = Control->Capacity;
	TSet<FGuid> InstanceIds;
	for (const FKnightInventoryRecord& Record : ReplicatedState.Records)
	{
		if (Record.bIsControl || Record.Revision != Candidate.Revision)
		{
			continue;
		}
		if (!Record.Item.ValidateIdentityAndCount().IsSuccess() || InstanceIds.Contains(Record.Item.InstanceId))
		{
			return;
		}
		InstanceIds.Add(Record.Item.InstanceId);
		Candidate.Items.Add(Record.Item);
	}
	if (Candidate.Items.Num() != Control->ItemCount)
	{
		return;
	}
	SortItems(Candidate.Items);
	PublishedView = MoveTemp(Candidate);
	Control = nullptr;
	OnSnapshotChanged.Broadcast();
}

void UKnightInventoryComponent::SortItems(TArray<FKnightItemInstance>& Items)
{
	Items.Sort([](const FKnightItemInstance& Left, const FKnightItemInstance& Right)
		{
			return Left.InstanceId < Right.InstanceId;
		});
}

#undef LOCTEXT_NAMESPACE
