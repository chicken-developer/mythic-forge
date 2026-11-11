#include "Inventory/GolemInventoryComponent.h"

#include "Models/GolemGearModels.h"

#include "Assets/GolemAssetManager.h"
#include "Definitions/GolemItemDefinition.h"
#include "Types/GolemStableIds.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "GolemInventoryComponent"

bool FGolemInventoryCapability::IsValid() const
{
	return Owner.IsValid() && Token.IsValid();
}

bool FGolemInventoryArray::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
{
	return FFastArraySerializer::FastArrayDeltaSerialize<FGolemInventoryRecord, FGolemInventoryArray>(
		Records, DeltaParams, *this);
}

void FGolemInventoryArray::PostReplicatedReceive(
	const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (UGolemInventoryComponent* Component = Owner.Get())
	{
		Component->HandleReplicatedStateReady();
	}
}

UGolemInventoryComponent::UGolemInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGolemInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGolemInventoryComponent, ReplicatedState);
}

FGolemInventoryView UGolemInventoryComponent::GetView() const
{
	return PublishedView;
}

bool UGolemInventoryComponent::IsInitialized() const
{
	return PublishedView.IsInitialized();
}

FGolemResult UGolemInventoryComponent::InitializeInventory(int32 InCapacity)
{
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsReserved())
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("InventoryInitialize"),
			LOCTEXT("InventoryReserved", "The inventory is reserved by an atomic domain transaction."));
	}
	if (IsInitialized())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("InventoryInitialize"),
			LOCTEXT("AlreadyInitialized", "An inventory cannot be reinitialized or reset by replacing its identity."));
	}
	if (InCapacity < 1 || InCapacity > GolemItems::MaximumInventoryCapacity)
	{
		return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("InventoryInitialize"),
			LOCTEXT("InvalidCapacity", "Inventory capacity is outside the supported finite range."));
	}
	PublishedView.InventoryId = FGuid::NewGuid();
	PublishedView.Capacity = InCapacity;
	PublishedView.Revision = 1;
	RefreshReplicatedState();
	OnSnapshotChanged.Broadcast();
	return FGolemResult::Success();
}

FGolemResult UGolemInventoryComponent::InitializeFixtureCatalog(TConstArrayView<UGolemItemDefinition*> Definitions)
{
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsInitialized() || bHasFixtureCatalog || IsReserved())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("InventoryFixtureCatalog"),
			LOCTEXT("FixtureCatalogLocked", "A fixture catalog may be configured only once before inventory initialization."));
	}
	if (Definitions.IsEmpty() || Definitions.Num() > GolemItems::MaximumInventoryCapacity)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("InventoryFixtureCatalog"),
			LOCTEXT("FixtureCatalogSize", "A fixture catalog requires a finite nonempty set of actual item definitions."));
	}
	TSet<FPrimaryAssetId> Ids;
	for (const UGolemItemDefinition* Definition : Definitions)
	{
		if (!IsValid(Definition))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("InventoryFixtureCatalog"),
				LOCTEXT("NullFixtureDefinition", "A fixture catalog cannot contain a null or destroyed definition."));
		}
		const FGolemResult Validation = Definition->ValidateItem();
		if (!Validation.IsSuccess())
		{
			return Validation;
		}
		if (Ids.Contains(Definition->GetPrimaryAssetId()))
		{
			return FGolemResult::Failure(EGolemError::DuplicateId, TEXT("InventoryFixtureCatalog"),
				LOCTEXT("DuplicateFixtureDefinition", "A fixture catalog cannot resolve duplicated definition IDs."));
		}
		Ids.Add(Definition->GetPrimaryAssetId());
	}
	TMap<FPrimaryAssetId, TObjectPtr<UGolemItemDefinition>> Copies;
	for (UGolemItemDefinition* Definition : Definitions)
	{
		UGolemItemDefinition* Copy = DuplicateObject<UGolemItemDefinition>(Definition, this);
		if (!IsValid(Copy) || !Copy->ValidateItem().IsSuccess()
			|| Copy->GetPrimaryAssetId() != Definition->GetPrimaryAssetId())
		{
			return FGolemResult::Failure(EGolemError::LoadFailed, TEXT("InventoryFixtureCatalog"),
				LOCTEXT("FixtureCopyFailed", "A validated item definition could not be copied into the immutable fixture catalog."));
		}
		Copy->SetFlags(RF_Transient);
		Copies.Add(Copy->GetPrimaryAssetId(), Copy);
	}
	FixtureDefinitions = MoveTemp(Copies);
	bHasFixtureCatalog = true;
	return FGolemResult::Success();
}

FGolemInventoryTransactionResult UGolemInventoryComponent::TryApplyTransaction(const FGolemInventoryRequest& Request)
{
	FGolemInventoryTransactionResult Outcome;
	Outcome.Result = ValidateAuthority();
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	if (IsReserved())
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::Conflict, TEXT("InventoryTransaction"),
			LOCTEXT("TransactionReserved", "A coordinated transaction currently reserves this inventory."));
		return Outcome;
	}
	if (!IsInitialized())
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::InvalidState, TEXT("InventoryTransaction"),
			LOCTEXT("InventoryNotInitialized", "Inventory transactions require an initialized authoritative inventory."));
		return Outcome;
	}
	if (const int32* ReceiptIndex = ReceiptIndices.Find(Request.RequestId))
	{
		const FGolemInventoryReceipt& Receipt = Receipts[*ReceiptIndex];
		if (!(Receipt.Request == Request))
		{
			Outcome.Result = FGolemResult::Failure(EGolemError::Conflict, TEXT("InventoryTransaction"),
				LOCTEXT("ChangedReplayPayload", "This request GUID already committed a different exact transaction payload."));
			return Outcome;
		}
		Outcome.Result = FGolemResult::Success();
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
		Outcome.Result = FGolemResult::Failure(EGolemError::InvalidRevision, TEXT("InventoryTransaction"),
			LOCTEXT("StaleRevision", "Inventory changed before this transaction; read its current revision before retrying."));
		return Outcome;
	}
	if (Receipts.Num() >= GolemItems::MaximumTransactionReceipts)
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("InventoryReceiptJournal"),
			LOCTEXT("ReceiptJournalFull", "The bounded transaction journal is full; writes cannot discard replay protection."));
		return Outcome;
	}
	TArray<FGolemItemInstance> CandidateItems;
	TSet<FGuid> CandidateIds;
	if (Request.Mutations.ContainsByPredicate([](const FGolemInventoryMutation& Mutation)
	{
		return Mutation.Operation == EGolemInventoryOperation::Update || Mutation.Item.LocationId != GolemGear::InventoryLocation;
	}))
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::NotOwned, TEXT("InventoryCoordinator"),
			LOCTEXT("CoordinatorUpdate", "Equipment locations and ranks change only through the authoritative RPG coordinator."));
		return Outcome;
	}
	Outcome.Result = BuildCandidate(PublishedView.Items, UsedInstanceIds, PublishedView.Capacity,
		Request, CandidateItems, CandidateIds);
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	FGolemInventoryReceipt Receipt;
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

FGolemResult UGolemInventoryComponent::CaptureSnapshot(FGolemInventorySnapshot& OutSnapshot) const
{
	OutSnapshot = FGolemInventorySnapshot();
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (!IsInitialized())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("InventorySnapshot"),
			LOCTEXT("SnapshotNotInitialized", "Only an initialized inventory can capture a persistence fragment."));
	}
	OutSnapshot.InventoryId = PublishedView.InventoryId;
	OutSnapshot.Capacity = PublishedView.Capacity;
	OutSnapshot.Revision = PublishedView.Revision;
	OutSnapshot.Items = PublishedView.Items;
	OutSnapshot.Receipts = Receipts;
	return FGolemResult::Success();
}

FGolemResult UGolemInventoryComponent::ValidateSnapshot(const FGolemInventorySnapshot& Snapshot,
	TArray<FGolemItemInstance>& OutItems, TSet<FGuid>& OutIds, TMap<FGuid, int32>& OutReceipts) const
{
	if ((Snapshot.SchemaVersion != 1 && Snapshot.SchemaVersion != GolemItems::InventorySnapshotVersion)
		|| !Snapshot.InventoryId.IsValid()
		|| Snapshot.Capacity < 1 || Snapshot.Capacity > GolemItems::MaximumInventoryCapacity
		|| Snapshot.Receipts.Num() > GolemItems::MaximumTransactionReceipts)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("InventoryRestore"),
			LOCTEXT("InvalidSnapshotHeader", "The inventory snapshot has an invalid version, identity, capacity or journal bound."));
	}
	if (Snapshot.Revision != static_cast<int64>(Snapshot.Receipts.Num()) + 1)
	{
		return FGolemResult::Failure(EGolemError::InvalidRevision, TEXT("InventoryRestore"),
			LOCTEXT("InvalidSnapshotRevision", "The claimed inventory revision does not match its complete successful journal."));
	}
	const FGolemResult ItemsResult = ValidateItems(Snapshot.Items, Snapshot.Capacity);
	if (!ItemsResult.IsSuccess())
	{
		return ItemsResult;
	}
	TArray<FGolemItemInstance> ReconstructedItems;
	TSet<FGuid> ReconstructedIds;
	TMap<FGuid, int32> ReconstructedReceipts;
	int64 ReconstructedRevision = 1;
	for (int32 Index = 0; Index < Snapshot.Receipts.Num(); ++Index)
	{
		const FGolemInventoryReceipt& Receipt = Snapshot.Receipts[Index];
		const FGolemResult Shape = Receipt.Request.ValidateShape();
		if (!Shape.IsSuccess())
		{
			return Shape;
		}
		if (ReconstructedReceipts.Contains(Receipt.Request.RequestId))
		{
			return FGolemResult::Failure(EGolemError::DuplicateId, TEXT("InventoryRestore"),
				LOCTEXT("DuplicateSnapshotReceipt", "The successful journal contains a duplicated request GUID."));
		}
		if (Receipt.Request.ExpectedRevision != ReconstructedRevision
			|| Receipt.CommittedRevision != ReconstructedRevision + 1)
		{
			return FGolemResult::Failure(EGolemError::InvalidRevision, TEXT("InventoryRestore"),
				LOCTEXT("InvalidReceiptRevision", "Successful receipts must form one ordered, contiguous revision history."));
		}
		TArray<FGolemItemInstance> CandidateItems;
		TSet<FGuid> CandidateIds;
		const FGolemResult CandidateResult = BuildCandidate(ReconstructedItems, ReconstructedIds,
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
	TArray<FGolemItemInstance> ClaimedItems = Snapshot.Items;
	SortItems(ClaimedItems);
	if (ReconstructedItems != ClaimedItems)
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("InventoryRestore"),
			LOCTEXT("SnapshotStateMismatch", "The claimed final item identities and counts disagree with the actual transaction history."));
	}
	OutItems = MoveTemp(ReconstructedItems);
	OutReceipts = MoveTemp(ReconstructedReceipts);
	OutIds = MoveTemp(ReconstructedIds);
	return FGolemResult::Success();
}

FGolemResult UGolemInventoryComponent::TryRestoreSnapshot(const FGolemInventorySnapshot& Snapshot)
{
	FGolemInventoryCapability Capability;
	FGolemResult Result = PrepareRestore(Snapshot, Capability);
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

bool UGolemInventoryComponent::IsReserved() const
{
	return PreparedToken.IsValid();
}

FGolemResult UGolemInventoryComponent::ValidateCapability(const FGolemInventoryCapability& Capability) const
{
	if (Capability.Owner.Get() != this || !Capability.Token.IsValid() || Capability.Token != PreparedToken)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("InventoryCapability"),
			LOCTEXT("InvalidCapability", "This capability does not own the inventory's current reservation."));
	}
	return FGolemResult::Success();
}

FGolemResult UGolemInventoryComponent::ReserveRead(const FGuid& InventoryId, int64 ExpectedRevision,
	FGolemInventoryCapability& OutCapability)
{
	OutCapability = FGolemInventoryCapability();
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsReserved())
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("InventoryReserve"),
			LOCTEXT("ReadReserved", "Another command already reserves this inventory."));
	}
	if (!IsInitialized() || InventoryId != PublishedView.InventoryId)
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("InventoryReserve"),
			LOCTEXT("ReadWrongInventory", "The read reservation requires the current initialized inventory identity."));
	}
	if (ExpectedRevision != PublishedView.Revision)
	{
		return FGolemResult::Failure(EGolemError::InvalidRevision, TEXT("InventoryReserve"),
			LOCTEXT("ReadStaleRevision", "The inventory revision changed before this command was prepared."));
	}
	PreparedToken = FGuid::NewGuid();
	PreparedView = PublishedView;
	PreparedIds = UsedInstanceIds;
	OutCapability.Owner = this;
	OutCapability.Token = PreparedToken;
	return FGolemResult::Success();
}

FGolemResult UGolemInventoryComponent::PrepareTransaction(const FGolemInventoryRequest& Request,
	FGolemInventoryCapability& OutCapability)
{
	OutCapability = FGolemInventoryCapability();
	if (IsReserved())
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("InventoryPrepare"),
			LOCTEXT("PrepareReserved", "Another command already reserves this inventory."));
	}
	const FGolemResult Shape = Request.ValidateShape();
	if (!Shape.IsSuccess())
	{
		return Shape;
	}
	if (!bCoordinatorPreparing && Request.Mutations.ContainsByPredicate([](const FGolemInventoryMutation& Mutation)
	{
		return Mutation.Operation == EGolemInventoryOperation::Update || Mutation.Item.LocationId != GolemGear::InventoryLocation;
	}))
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("InventoryCoordinator"),
			LOCTEXT("CoordinatorPrepare", "Assigned item updates require the existing RPG coordinator's private prepare path."));
	}
	if (ReceiptIndices.Contains(Request.RequestId))
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("InventoryPrepare"),
			LOCTEXT("PrepareAlreadyCommitted", "A prepared write must be new; the coordinator authenticates successful replays."));
	}
	if (Receipts.Num() >= GolemItems::MaximumTransactionReceipts)
	{
		return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("InventoryPrepare"),
			LOCTEXT("PreparedJournalFull", "The complete successful journal reached its finite receipt limit."));
	}
	FGolemResult Result = ReserveRead(PublishedView.InventoryId, Request.ExpectedRevision, OutCapability);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Result = BuildCandidate(PublishedView.Items, UsedInstanceIds, PublishedView.Capacity,
		Request, PreparedView.Items, PreparedIds);
	if (!Result.IsSuccess())
	{
		ResetPrepared();
		OutCapability = FGolemInventoryCapability();
		return Result;
	}
	PreparedRequest = Request;
	PreparedView.Revision = PublishedView.Revision + 1;
	bPreparedWrite = true;
	return FGolemResult::Success();
}

FGolemResult UGolemInventoryComponent::PrepareCoordinatorTransaction(const FGolemInventoryRequest& Request,
	FGolemInventoryCapability& OutCapability)
{
	TGuardValue<bool> Scope(bCoordinatorPreparing, true);
	return PrepareTransaction(Request, OutCapability);
}

FGolemResult UGolemInventoryComponent::PrepareRestore(const FGolemInventorySnapshot& Snapshot,
	FGolemInventoryCapability& OutCapability)
{
	OutCapability = FGolemInventoryCapability();
	const FGolemResult Authority = ValidateAuthority();
	if (!Authority.IsSuccess())
	{
		return Authority;
	}
	if (IsReserved())
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("InventoryRestore"),
			LOCTEXT("RestoreReserved", "A reserved inventory cannot prepare reconstruction."));
	}
	if (IsInitialized())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("InventoryRestore"),
			LOCTEXT("RestoreFreshOnly", "Reconstruction requires a fresh inventory and never overwrites live state."));
	}
	TArray<FGolemItemInstance> Items;
	TSet<FGuid> Ids;
	TMap<FGuid, int32> Indices;
	const FGolemResult Validation = ValidateSnapshot(Snapshot, Items, Ids, Indices);
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
	return FGolemResult::Success();
}

FGolemResult UGolemInventoryComponent::GetPreparedView(const FGolemInventoryCapability& Capability,
	FGolemInventoryView& OutView) const
{
	OutView = FGolemInventoryView();
	const FGolemResult Validation = ValidateCapability(Capability);
	if (Validation.IsSuccess())
	{
		OutView = PreparedView;
	}
	return Validation;
}

FGolemResult UGolemInventoryComponent::CommitPrepared(const FGolemInventoryCapability& Capability)
{
	const FGolemResult Validation = ValidateCapability(Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (bPreparedCommitted)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("InventoryCommit"),
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
			FGolemInventoryReceipt Receipt;
			Receipt.Request = PreparedRequest;
			Receipt.CommittedRevision = PublishedView.Revision;
			ReceiptIndices.Add(Receipt.Request.RequestId, Receipts.Num());
			Receipts.Add(MoveTemp(Receipt));
		}
		RefreshReplicatedState();
	}
	bPreparedCommitted = true;
	return FGolemResult::Success();
}

FGolemResult UGolemInventoryComponent::AbortPrepared(const FGolemInventoryCapability& Capability)
{
	const FGolemResult Validation = ValidateCapability(Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (bPreparedCommitted)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("InventoryAbort"),
			LOCTEXT("CannotAbortCommitted", "A committed domain transaction cannot be rolled back with a fresh snapshot."));
	}
	ResetPrepared();
	return FGolemResult::Success();
}

FGolemResult UGolemInventoryComponent::PublishPrepared(const FGolemInventoryCapability& Capability,
	bool bReleaseReservation)
{
	const FGolemResult Validation = ValidateCapability(Capability);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (!bPreparedCommitted)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("InventoryPublish"),
			LOCTEXT("CannotPublishUncommitted", "A prepared candidate must commit before publication."));
	}
	if (bPreparedPublishing || bPreparedPublished)
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("InventoryPublish"),
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
	return FGolemResult::Success();
}

void UGolemInventoryComponent::ResetPrepared()
{
	PreparedToken.Invalidate();
	PreparedView = FGolemInventoryView();
	PreparedIds.Reset();
	PreparedRequest = FGolemInventoryRequest();
	PreparedSnapshot = FGolemInventorySnapshot();
	PreparedReceiptIndices.Reset();
	bPreparedWrite = false;
	bPreparedRestore = false;
	bPreparedCommitted = false;
	bPreparedPublishing = false;
	bPreparedPublished = false;
}

void UGolemInventoryComponent::OnRegister()
{
	Super::OnRegister();
	ReplicatedState.Owner = this;
}

void UGolemInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	ResetPrepared();
	ReplicatedState.Owner.Reset();
	Super::EndPlay(EndPlayReason);
}

FGolemResult UGolemInventoryComponent::ValidateAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	if (!IsInGameThread() || bEndingPlay || !IsValid(OwnerActor) || OwnerActor->IsActorBeingDestroyed()
		|| World == nullptr || World->bIsTearingDown)
	{
		return FGolemResult::Failure(EGolemError::ShuttingDown, TEXT("InventoryAuthority"),
			LOCTEXT("InventoryLifetimeEnded", "The inventory owner or its world is unavailable for this command."));
	}
	if (!OwnerActor->HasAuthority() || World->GetNetMode() == NM_Client)
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("InventoryAuthority"),
			LOCTEXT("InventoryServerOnly", "Only the authoritative campaign server may mutate or persist shared inventory."));
	}
	return FGolemResult::Success();
}

FGolemResult UGolemInventoryComponent::ResolveItemDefinition(const FPrimaryAssetId& DefinitionId,
	const UGolemItemDefinition*& OutDefinition) const
{
	OutDefinition = nullptr;
	if (!GolemIdentity::IsValidDefinitionId(DefinitionId)
		|| DefinitionId.PrimaryAssetType != FPrimaryAssetType(TEXT("GolemItem")))
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("InventoryDefinition"),
			LOCTEXT("InvalidInventoryDefinition", "Inventory requires a valid stable GolemItem definition ID."));
	}
	if (bHasFixtureCatalog)
	{
		const TObjectPtr<UGolemItemDefinition>* Definition = FixtureDefinitions.Find(DefinitionId);
		if (Definition == nullptr || !IsValid(Definition->Get()))
		{
			return FGolemResult::Failure(EGolemError::NotFound, FName(*DefinitionId.ToString()),
				LOCTEXT("FixtureDefinitionMissing", "No validated immutable fixture definition matches this item identity."));
		}
		OutDefinition = Definition->Get();
	}
	else
	{
		UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
		if (Manager == nullptr)
		{
			return FGolemResult::Failure(EGolemError::LoadFailed, TEXT("InventoryDefinition"),
				LOCTEXT("ItemAssetManagerUnavailable", "The configured Golem Asset Manager is unavailable."));
		}
		UGolemDefinition* Definition = nullptr;
		const FGolemResult Resolution = Manager->ResolveDefinition(DefinitionId,
			UGolemItemDefinition::StaticClass(), Definition);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		OutDefinition = Cast<UGolemItemDefinition>(Definition);
	}
	if (OutDefinition == nullptr || OutDefinition->GetPrimaryAssetId() != DefinitionId)
	{
		OutDefinition = nullptr;
		return FGolemResult::Failure(EGolemError::TypeMismatch, FName(*DefinitionId.ToString()),
			LOCTEXT("WrongResolvedDefinition", "The resolved asset is not the exact requested typed item definition."));
	}
	const FGolemResult Validation = OutDefinition->ValidateItem();
	if (!Validation.IsSuccess())
	{
		OutDefinition = nullptr;
	}
	return Validation;
}

FGolemResult UGolemInventoryComponent::BuildCandidate(const TArray<FGolemItemInstance>& ExistingItems,
	const TSet<FGuid>& ExistingInstanceIds, int32 InCapacity, const FGolemInventoryRequest& Request,
	TArray<FGolemItemInstance>& OutItems, TSet<FGuid>& OutInstanceIds) const
{
	OutItems = ExistingItems;
	OutInstanceIds = ExistingInstanceIds;
	for (const FGolemInventoryMutation& Mutation : Request.Mutations)
	{
		const UGolemItemDefinition* Definition = nullptr;
		const FGolemResult Resolution = ResolveItemDefinition(Mutation.Item.DefinitionId, Definition);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		const int32 ExistingIndex = OutItems.IndexOfByPredicate([&Mutation](const FGolemItemInstance& Item)
			{
				return Item.InstanceId == Mutation.Item.InstanceId;
			});
		if (ExistingIndex != INDEX_NONE && OutItems[ExistingIndex].DefinitionId != Mutation.Item.DefinitionId)
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("InventoryInstance"),
				LOCTEXT("InstanceDefinitionMismatch", "An instance GUID cannot change its stable definition identity."));
		}
		if (Mutation.ExpectedItemRevision > 0 && (ExistingIndex == INDEX_NONE
			|| OutItems[ExistingIndex].ItemRevision != Mutation.ExpectedItemRevision))
		{
			return FGolemResult::Failure(EGolemError::InvalidRevision, TEXT("InventoryItemRevision"),
				LOCTEXT("ItemRevision", "The exact item instance changed before this transaction."));
		}
		if (Mutation.Operation == EGolemInventoryOperation::Add)
		{
			if (ExistingIndex == INDEX_NONE)
			{
				if (OutInstanceIds.Contains(Mutation.Item.InstanceId))
				{
					return FGolemResult::Failure(EGolemError::DuplicateId, TEXT("InventoryInstance"),
						LOCTEXT("RetiredInstanceId", "A removed instance GUID cannot be reused for a newly created item."));
				}
				OutItems.Add(Mutation.Item);
				OutInstanceIds.Add(Mutation.Item.InstanceId);
			}
			else
			{
				if (OutItems[ExistingIndex].LocationId != GolemGear::InventoryLocation)
				{
					return FGolemResult::Failure(EGolemError::NotOwned, TEXT("InventoryAddLocation"),
						LOCTEXT("AddLocation", "Only inventory stacks may receive additional counts."));
				}
				const int64 CombinedCount = static_cast<int64>(OutItems[ExistingIndex].Count) + Mutation.Item.Count;
				if (CombinedCount > Definition->GetMaximumStack())
				{
					return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("InventoryStack"),
						LOCTEXT("StackExceeded", "The transaction would exceed the authored maximum stack."));
				}
				OutItems[ExistingIndex].Count = static_cast<int32>(CombinedCount);
				if (Mutation.ExpectedItemRevision > 0)
				{
					++OutItems[ExistingIndex].ItemRevision;
				}
			}
		}
		else if (Mutation.Operation == EGolemInventoryOperation::Update)
		{
			if (ExistingIndex == INDEX_NONE || Mutation.Item.Count != OutItems[ExistingIndex].Count
				|| Mutation.Item.ItemRevision != OutItems[ExistingIndex].ItemRevision + 1)
			{
				return FGolemResult::Failure(EGolemError::InvalidRevision, TEXT("InventoryUpdate"),
					LOCTEXT("Update", "Location and upgrade updates retain identity/count and advance one exact revision."));
			}
			OutItems[ExistingIndex] = Mutation.Item;
		}
		else
		{
			if (AssignedInstanceIds.Contains(Mutation.Item.InstanceId)
				|| (ExistingIndex != INDEX_NONE && (OutItems[ExistingIndex].LocationId != GolemGear::InventoryLocation
					|| OutItems[ExistingIndex].RuneAttachmentId.IsValid())))
			{
				return FGolemResult::Failure(EGolemError::NotOwned, TEXT("InventoryAssignment"),
					LOCTEXT("AssignedRemoval", "An assigned equipment instance must be unequipped before removal."));
			}
			if (ExistingIndex == INDEX_NONE)
			{
				return FGolemResult::Failure(EGolemError::NotFound, TEXT("InventoryInstance"),
					LOCTEXT("MissingRemovalInstance", "The exact item instance to remove does not exist in this inventory."));
			}
			if (Mutation.Item.Count > OutItems[ExistingIndex].Count)
			{
				return FGolemResult::Failure(EGolemError::InsufficientResources, TEXT("InventoryCount"),
					LOCTEXT("RemovalCountExceeded", "The requested removal exceeds the current item count."));
			}
			OutItems[ExistingIndex].Count -= Mutation.Item.Count;
			if (Mutation.ExpectedItemRevision > 0)
			{
				++OutItems[ExistingIndex].ItemRevision;
			}
			if (OutItems[ExistingIndex].Count == 0)
			{
				OutItems.RemoveAt(ExistingIndex);
			}
		}
	}
	const FGolemResult Validation = ValidateItems(OutItems, InCapacity);
	if (Validation.IsSuccess())
	{
		SortItems(OutItems);
	}
	return Validation;
}

FGolemResult UGolemInventoryComponent::ValidateItems(const TArray<FGolemItemInstance>& Items, int32 InCapacity) const
{
	int32 BagCount = 0;
	for (const FGolemItemInstance& Item : Items)
	{
		BagCount += Item.LocationId == GolemGear::InventoryLocation ? 1 : 0;
	}
	if (InCapacity < 1 || InCapacity > GolemItems::MaximumInventoryCapacity || BagCount > InCapacity
		|| Items.Num() > GolemItems::MaximumOwnedInstances)
	{
		return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("InventoryCapacity"),
			LOCTEXT("InventoryCapacityExceeded", "The complete candidate inventory exceeds its finite instance capacity."));
	}
	TSet<FGuid> Ids;
	for (const FGolemItemInstance& Item : Items)
	{
		const FGolemResult Identity = Item.ValidateIdentityAndCount();
		if (!Identity.IsSuccess())
		{
			return Identity;
		}
		if (Ids.Contains(Item.InstanceId))
		{
			return FGolemResult::Failure(EGolemError::DuplicateId, TEXT("InventoryInstance"),
				LOCTEXT("DuplicateInventoryInstance", "Inventory cannot contain two entries with the same persistent instance GUID."));
		}
		Ids.Add(Item.InstanceId);
		const UGolemItemDefinition* Definition = nullptr;
		const FGolemResult Resolution = ResolveItemDefinition(Item.DefinitionId, Definition);
		if (!Resolution.IsSuccess())
		{
			return Resolution;
		}
		if (Item.Count > Definition->GetMaximumStack())
		{
			return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("InventoryStack"),
				LOCTEXT("InvalidCandidateStack", "An item count exceeds its validated definition's maximum stack."));
		}
		if ((Item.UpgradeRank > 0 && Definition->GetMetadata().UpgradeTrack != TEXT("RareOne"))
			|| (Item.RuneAttachmentId.IsValid() && Definition->GetMetadata().Subtype != EGolemEquipmentSubtype::Hammer)
			|| (Definition->GetMetadata().bUniqueOwned && Items.ContainsByPredicate([&Item](const FGolemItemInstance& Other)
			{
				return Other.InstanceId != Item.InstanceId && Other.DefinitionId == Item.DefinitionId;
			})))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("InventoryItemMetadata"),
				LOCTEXT("Metadata", "Upgrade, weapon socket and unique-owned item values must match their real definitions."));
		}
	}
	return FGolemResult::Success();
}

void UGolemInventoryComponent::RefreshReplicatedState()
{
	ReplicatedState.Owner = this;
	for (int32 Index = ReplicatedState.Records.Num() - 1; Index >= 0; --Index)
	{
		const FGolemInventoryRecord& Record = ReplicatedState.Records[Index];
		if (!Record.bIsControl && !PublishedView.Items.ContainsByPredicate([&Record](const FGolemItemInstance& Item)
			{
				return Item.InstanceId == Record.Item.InstanceId;
			}))
		{
			ReplicatedState.Records.RemoveAt(Index);
			ReplicatedState.MarkArrayDirty();
		}
	}
	int32 ControlIndex = ReplicatedState.Records.IndexOfByPredicate([](const FGolemInventoryRecord& Record)
		{
			return Record.bIsControl;
		});
	if (ControlIndex == INDEX_NONE)
	{
		ControlIndex = ReplicatedState.Records.AddDefaulted();
	}
	FGolemInventoryRecord& Control = ReplicatedState.Records[ControlIndex];
	Control.bIsControl = true;
	Control.InventoryId = PublishedView.InventoryId;
	Control.Capacity = PublishedView.Capacity;
	Control.ItemCount = PublishedView.Items.Num();
	Control.Revision = PublishedView.Revision;
	ReplicatedState.MarkItemDirty(Control);
	for (const FGolemItemInstance& Item : PublishedView.Items)
	{
		int32 RecordIndex = ReplicatedState.Records.IndexOfByPredicate([&Item](const FGolemInventoryRecord& Record)
			{
				return !Record.bIsControl && Record.Item.InstanceId == Item.InstanceId;
			});
		if (RecordIndex == INDEX_NONE)
		{
			RecordIndex = ReplicatedState.Records.AddDefaulted();
		}
		FGolemInventoryRecord& Record = ReplicatedState.Records[RecordIndex];
		Record.Item = Item;
		Record.Revision = PublishedView.Revision;
		ReplicatedState.MarkItemDirty(Record);
	}
	GetOwner()->ForceNetUpdate();
}

void UGolemInventoryComponent::HandleReplicatedStateReady()
{
	if (bEndingPlay)
	{
		return;
	}
	const FGolemInventoryRecord* Control = nullptr;
	for (const FGolemInventoryRecord& Record : ReplicatedState.Records)
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
		|| Control->Capacity > GolemItems::MaximumInventoryCapacity
		|| Control->ItemCount < 0 || Control->ItemCount > GolemItems::MaximumOwnedInstances
		|| (IsInitialized() && Control->InventoryId != PublishedView.InventoryId))
	{
		return;
	}
	FGolemInventoryView Candidate;
	Candidate.InventoryId = Control->InventoryId;
	Candidate.Revision = Control->Revision;
	Candidate.Capacity = Control->Capacity;
	TSet<FGuid> InstanceIds;
	for (const FGolemInventoryRecord& Record : ReplicatedState.Records)
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

void UGolemInventoryComponent::SortItems(TArray<FGolemItemInstance>& Items)
{
	Items.Sort([](const FGolemItemInstance& Left, const FGolemItemInstance& Right)
		{
			return Left.InstanceId < Right.InstanceId;
		});
}

#undef LOCTEXT_NAMESPACE
