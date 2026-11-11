#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Models/GolemItemModels.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "GolemInventoryComponent.generated.h"

class UGolemInventoryComponent;
class UGolemItemDefinition;
class UGolemRPGComponent;

/** Copyable opaque synchronous reservation. Only the issuing inventory can authenticate it. */
class GOLEMITEMS_API FGolemInventoryCapability
{
public:
	bool IsValid() const;

private:
	friend class UGolemInventoryComponent;
	TWeakObjectPtr<UGolemInventoryComponent> Owner;
	FGuid Token;
};

/** Internal replication record. The control record and every live item carry the same commit revision. */
USTRUCT()
struct GOLEMITEMS_API FGolemInventoryRecord : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsControl = false;

	UPROPERTY()
	int64 Revision = 0;

	UPROPERTY()
	FGolemItemInstance Item;

	UPROPERTY()
	FGuid InventoryId;

	UPROPERTY()
	int32 Capacity = 0;

	UPROPERTY()
	int32 ItemCount = 0;
};

USTRUCT()
struct GOLEMITEMS_API FGolemInventoryArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGolemInventoryRecord> Records;

	UPROPERTY(NotReplicated)
	TWeakObjectPtr<UGolemInventoryComponent> Owner;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams);
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);
};

template <>
struct TStructOpsTypeTraits<FGolemInventoryArray> : public TStructOpsTypeTraitsBase2<FGolemInventoryArray>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

DECLARE_MULTICAST_DELEGATE(FOnGolemInventorySnapshotChanged);

/** Shared server-owned domain. No client RPC is exposed; the host composition validates caller ownership. */
UCLASS(ClassGroup = (Golem), meta = (BlueprintSpawnableComponent))
class GOLEMITEMS_API UGolemInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGolemInventoryComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGolemInventoryView GetView() const;
	bool IsInitialized() const;
	FGolemResult InitializeInventory(int32 InCapacity);

	/** Explicit isolated fixture setup, once before initialization. Private strong copies cannot be edited by the caller. */
	FGolemResult InitializeFixtureCatalog(TConstArrayView<UGolemItemDefinition*> Definitions);
	FGolemInventoryTransactionResult TryApplyTransaction(const FGolemInventoryRequest& Request);
	FGolemResult ReserveRead(const FGuid& InventoryId, int64 ExpectedRevision, FGolemInventoryCapability& OutCapability);
	FGolemResult PrepareTransaction(const FGolemInventoryRequest& Request, FGolemInventoryCapability& OutCapability);
	FGolemResult PrepareRestore(const FGolemInventorySnapshot& Snapshot, FGolemInventoryCapability& OutCapability);
	FGolemResult GetPreparedView(const FGolemInventoryCapability& Capability, FGolemInventoryView& OutView) const;
	FGolemResult CommitPrepared(const FGolemInventoryCapability& Capability);
	FGolemResult AbortPrepared(const FGolemInventoryCapability& Capability);
	/** Publish while still reserved; subscriber commands fail Conflict until this call returns. */
	FGolemResult PublishPrepared(const FGolemInventoryCapability& Capability, bool bReleaseReservation = true);
	bool IsReserved() const;
	FGolemResult CaptureSnapshot(FGolemInventorySnapshot& OutSnapshot) const;
	/** Fresh authority only; the complete successful journal is validated and replayed before any state is published. */
	FGolemResult TryRestoreSnapshot(const FGolemInventorySnapshot& Snapshot);
	FOnGolemInventorySnapshotChanged OnSnapshotChanged;

protected:
	virtual void OnRegister() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend struct FGolemInventoryArray;
	friend class UGolemRPGComponent;
	FGolemResult ValidateCapability(const FGolemInventoryCapability& Capability) const;
	FGolemResult PrepareCoordinatorTransaction(const FGolemInventoryRequest& Request,
		FGolemInventoryCapability& OutCapability);
	FGolemResult ValidateSnapshot(const FGolemInventorySnapshot& Snapshot,
		TArray<FGolemItemInstance>& OutItems, TSet<FGuid>& OutIds, TMap<FGuid, int32>& OutReceipts) const;
	void ResetPrepared();
	FGolemResult ValidateAuthority() const;
	FGolemResult ResolveItemDefinition(const FPrimaryAssetId& DefinitionId,
		const UGolemItemDefinition*& OutDefinition) const;
	FGolemResult BuildCandidate(const TArray<FGolemItemInstance>& ExistingItems,
		const TSet<FGuid>& ExistingInstanceIds, int32 InCapacity, const FGolemInventoryRequest& Request,
		TArray<FGolemItemInstance>& OutItems, TSet<FGuid>& OutInstanceIds) const;
	FGolemResult ValidateItems(const TArray<FGolemItemInstance>& Items, int32 InCapacity) const;
	void RefreshReplicatedState();
	void HandleReplicatedStateReady();
	static void SortItems(TArray<FGolemItemInstance>& Items);

	UPROPERTY(Replicated)
	FGolemInventoryArray ReplicatedState;

	UPROPERTY(Transient)
	TMap<FPrimaryAssetId, TObjectPtr<UGolemItemDefinition>> FixtureDefinitions;

	FGolemInventoryView PublishedView;
	TArray<FGolemInventoryReceipt> Receipts;
	TMap<FGuid, int32> ReceiptIndices;
	TSet<FGuid> UsedInstanceIds;
	TSet<FGuid> AssignedInstanceIds;
	FGuid PreparedToken;
	FGolemInventoryView PreparedView;
	TSet<FGuid> PreparedIds;
	FGolemInventoryRequest PreparedRequest;
	FGolemInventorySnapshot PreparedSnapshot;
	TMap<FGuid, int32> PreparedReceiptIndices;
	bool bPreparedWrite = false;
	bool bPreparedRestore = false;
	bool bPreparedCommitted = false;
	bool bPreparedPublishing = false;
	bool bPreparedPublished = false;
	bool bHasFixtureCatalog = false;
	bool bEndingPlay = false;
	bool bCoordinatorPreparing = false;
};
