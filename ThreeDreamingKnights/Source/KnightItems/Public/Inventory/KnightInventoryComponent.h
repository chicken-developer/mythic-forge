#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Models/KnightItemModels.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "KnightInventoryComponent.generated.h"

class UKnightInventoryComponent;
class UKnightItemDefinition;
class UKnightRPGComponent;

/** Copyable opaque synchronous reservation. Only the issuing inventory can authenticate it. */
class KNIGHTITEMS_API FKnightInventoryCapability
{
public:
	bool IsValid() const;

private:
	friend class UKnightInventoryComponent;
	TWeakObjectPtr<UKnightInventoryComponent> Owner;
	FGuid Token;
};

/** Internal replication record. The control record and every live item carry the same commit revision. */
USTRUCT()
struct KNIGHTITEMS_API FKnightInventoryRecord : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsControl = false;

	UPROPERTY()
	int64 Revision = 0;

	UPROPERTY()
	FKnightItemInstance Item;

	UPROPERTY()
	FGuid InventoryId;

	UPROPERTY()
	int32 Capacity = 0;

	UPROPERTY()
	int32 ItemCount = 0;
};

USTRUCT()
struct KNIGHTITEMS_API FKnightInventoryArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FKnightInventoryRecord> Records;

	UPROPERTY(NotReplicated)
	TWeakObjectPtr<UKnightInventoryComponent> Owner;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams);
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);
};

template <>
struct TStructOpsTypeTraits<FKnightInventoryArray> : public TStructOpsTypeTraitsBase2<FKnightInventoryArray>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

DECLARE_MULTICAST_DELEGATE(FOnKnightInventorySnapshotChanged);

/** Shared server-owned domain. No client RPC is exposed; the host composition validates caller ownership. */
UCLASS(ClassGroup = (Knight), meta = (BlueprintSpawnableComponent))
class KNIGHTITEMS_API UKnightInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKnightInventoryComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FKnightInventoryView GetView() const;
	bool IsInitialized() const;
	FKnightResult InitializeInventory(int32 InCapacity);

	/** Explicit isolated fixture setup, once before initialization. Private strong copies cannot be edited by the caller. */
	FKnightResult InitializeFixtureCatalog(TConstArrayView<UKnightItemDefinition*> Definitions);
	FKnightInventoryTransactionResult TryApplyTransaction(const FKnightInventoryRequest& Request);
	FKnightResult ReserveRead(const FGuid& InventoryId, int64 ExpectedRevision, FKnightInventoryCapability& OutCapability);
	FKnightResult PrepareTransaction(const FKnightInventoryRequest& Request, FKnightInventoryCapability& OutCapability);
	FKnightResult PrepareRestore(const FKnightInventorySnapshot& Snapshot, FKnightInventoryCapability& OutCapability);
	FKnightResult GetPreparedView(const FKnightInventoryCapability& Capability, FKnightInventoryView& OutView) const;
	FKnightResult CommitPrepared(const FKnightInventoryCapability& Capability);
	FKnightResult AbortPrepared(const FKnightInventoryCapability& Capability);
	/** Publish while still reserved; subscriber commands fail Conflict until this call returns. */
	FKnightResult PublishPrepared(const FKnightInventoryCapability& Capability, bool bReleaseReservation = true);
	bool IsReserved() const;
	FKnightResult CaptureSnapshot(FKnightInventorySnapshot& OutSnapshot) const;
	/** Fresh authority only; the complete successful journal is validated and replayed before any state is published. */
	FKnightResult TryRestoreSnapshot(const FKnightInventorySnapshot& Snapshot);
	FOnKnightInventorySnapshotChanged OnSnapshotChanged;

protected:
	virtual void OnRegister() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend struct FKnightInventoryArray;
	friend class UKnightRPGComponent;
	FKnightResult ValidateCapability(const FKnightInventoryCapability& Capability) const;
	FKnightResult ValidateSnapshot(const FKnightInventorySnapshot& Snapshot,
		TArray<FKnightItemInstance>& OutItems, TSet<FGuid>& OutIds, TMap<FGuid, int32>& OutReceipts) const;
	void ResetPrepared();
	FKnightResult ValidateAuthority() const;
	FKnightResult ResolveItemDefinition(const FPrimaryAssetId& DefinitionId,
		const UKnightItemDefinition*& OutDefinition) const;
	FKnightResult BuildCandidate(const TArray<FKnightItemInstance>& ExistingItems,
		const TSet<FGuid>& ExistingInstanceIds, int32 InCapacity, const FKnightInventoryRequest& Request,
		TArray<FKnightItemInstance>& OutItems, TSet<FGuid>& OutInstanceIds) const;
	FKnightResult ValidateItems(const TArray<FKnightItemInstance>& Items, int32 InCapacity) const;
	void RefreshReplicatedState();
	void HandleReplicatedStateReady();
	static void SortItems(TArray<FKnightItemInstance>& Items);

	UPROPERTY(Replicated)
	FKnightInventoryArray ReplicatedState;

	UPROPERTY(Transient)
	TMap<FPrimaryAssetId, TObjectPtr<UKnightItemDefinition>> FixtureDefinitions;

	FKnightInventoryView PublishedView;
	TArray<FKnightInventoryReceipt> Receipts;
	TMap<FGuid, int32> ReceiptIndices;
	TSet<FGuid> UsedInstanceIds;
	TSet<FGuid> AssignedInstanceIds;
	FGuid PreparedToken;
	FKnightInventoryView PreparedView;
	TSet<FGuid> PreparedIds;
	FKnightInventoryRequest PreparedRequest;
	FKnightInventorySnapshot PreparedSnapshot;
	TMap<FGuid, int32> PreparedReceiptIndices;
	bool bPreparedWrite = false;
	bool bPreparedRestore = false;
	bool bPreparedCommitted = false;
	bool bPreparedPublishing = false;
	bool bPreparedPublished = false;
	bool bHasFixtureCatalog = false;
	bool bEndingPlay = false;
};
