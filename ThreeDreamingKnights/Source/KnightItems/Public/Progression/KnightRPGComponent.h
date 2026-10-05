#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Models/KnightRPGModels.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "KnightRPGComponent.generated.h"

class AKnightItemPickup;
class UKnightDefinition;
class UKnightInventoryComponent;
class UKnightItemDefinition;
class UKnightSkillDefinition;
class UKnightSkillTreeDefinition;
class UKnightRPGComponent;

USTRUCT()
struct KNIGHTITEMS_API FKnightRPGRecord : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsControl = false;
	UPROPERTY()
	int64 Revision = 0;
	UPROPERTY()
	FGuid InventoryId;
	UPROPERTY()
	int64 InventoryRevision = 0;
	UPROPERTY()
	FPrimaryAssetId SkillTreeId;
	UPROPERTY()
	FKnightRPGKnightView Knight;
};

USTRUCT()
struct KNIGHTITEMS_API FKnightRPGArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FKnightRPGRecord> Records;
	UPROPERTY(NotReplicated)
	TWeakObjectPtr<UKnightRPGComponent> Owner;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams);
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);
};

template <>
struct TStructOpsTypeTraits<FKnightRPGArray> : public TStructOpsTypeTraitsBase2<FKnightRPGArray>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

DECLARE_MULTICAST_DELEGATE(FOnKnightRPGSnapshotChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightRPGCommandCompleted, const FKnightRPGCommandResult&);

/** Shared authority coordinates inventory values and three independent source-owned GAS projections. */
UCLASS(ClassGroup = (Knight), meta = (BlueprintSpawnableComponent))
class KNIGHTITEMS_API UKnightRPGComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKnightRPGComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FKnightResult InitializeRPG(UKnightInventoryComponent* InInventory, const FPrimaryAssetId& InSkillTreeId,
		TConstArrayView<FKnightRPGKnightRegistration> InRegistrations, int32 InitialPoints = 2);
	/** Clients bind the replicated sibling without granting or mutating anything. */
	FKnightResult BindReplicatedInventory(UKnightInventoryComponent* InInventory);
	FKnightResult InitializeFixtureCatalog(TConstArrayView<UKnightDefinition*> Definitions);
	FKnightRPGCommandResult ExecuteCommand(const FKnightRPGCallerContext& Caller, const FKnightRPGCommand& Command);
	FKnightResult RegisterPickup(AKnightItemPickup* Pickup);
	/** Composition may withdraw its exact unclaimed actor after a failed reward spawn batch. */
	FKnightResult UnregisterUnclaimedPickup(AKnightItemPickup* Pickup);
	FKnightRPGView GetView() const;
	FKnightRPGKnightView GetKnightView(const FKnightEntityId& KnightId) const;
	bool IsInitialized() const;
	bool IsTransactionInProgress() const;
	FKnightResult CaptureSnapshot(FKnightRPGSnapshot& OutSnapshot) const;
	/** Fresh inventory/coordinator only; journal reconstruction and all projections validate before commit. */
	FKnightResult TryRestoreSnapshot(UKnightInventoryComponent* InInventory, const FKnightRPGSnapshot& Snapshot,
		TConstArrayView<FKnightRPGKnightRegistration> InRegistrations);
	FOnKnightRPGSnapshotChanged OnSnapshotChanged;
	FOnKnightRPGCommandCompleted OnCommandCompleted;

protected:
	virtual void OnRegister() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend struct FKnightRPGArray;
	FKnightResult ValidateAuthority() const;
	FKnightResult ValidateCaller(const FKnightRPGCallerContext& Caller) const;
	FKnightResult ValidateRegistrations(TConstArrayView<FKnightRPGKnightRegistration> InRegistrations,
		const FKnightRPGView* ExpectedView = nullptr, bool bAllowMutationFence = false,
		bool bRequireFreshSource = true) const;
	FKnightResult ResolveDefinition(const FPrimaryAssetId& Id, UClass* ExpectedClass,
		const UKnightDefinition*& OutDefinition) const;
	FKnightResult ResolveSkills(const FPrimaryAssetId& TreeId);
	FKnightResult BuildCandidate(const FKnightRPGView& Existing, const FKnightInventoryView& InventoryView,
		const FKnightRPGCommand& Command, const FKnightItemInstance* PickedItem,
		const TArray<FKnightInventoryMutation>* HistoricalMutations, FKnightRPGView& OutView,
		FKnightInventoryRequest& OutInventoryRequest, FGuid& OutAffectedInstance) const;
	FKnightResult BuildProjection(FKnightRPGKnightView& Knight, const TArray<FKnightItemInstance>& Items,
		FKnightGASProjectionPlan& OutPlan) const;
	FKnightResult ValidateSnapshot(const FKnightRPGSnapshot& Snapshot, FKnightRPGView& OutView) const;
	void RefreshReplicatedState();
	void HandleReplicatedStateReady();
	void HandleInventoryChanged();
	void BindInventory(UKnightInventoryComponent* InInventory);
	static void SortView(FKnightRPGView& View);

	UPROPERTY(Replicated)
	FKnightRPGArray ReplicatedState;
	UPROPERTY(Transient)
	TObjectPtr<UKnightInventoryComponent> Inventory;
	UPROPERTY(Transient)
	TMap<FPrimaryAssetId, TObjectPtr<UKnightDefinition>> FixtureDefinitions;
	UPROPERTY(Transient)
	TMap<FPrimaryAssetId, TObjectPtr<UKnightSkillDefinition>> Skills;
	TMap<FKnightEntityId, FKnightRPGKnightRegistration> Registrations;
	TMap<FGuid, TWeakObjectPtr<AKnightItemPickup>> Pickups;
	TArray<FKnightRPGReceipt> Receipts;
	TMap<FGuid, int32> ReceiptIndices;
	FKnightRPGView PublishedView;
	FKnightRPGView InitialView;
	FKnightInventorySnapshot InitialInventory;
	FDelegateHandle InventoryChangedHandle;
	int32 InitialSkillPoints = 2;
	bool bHasFixtureCatalog = false;
	bool bTransactionInProgress = false;
	bool bEndingPlay = false;
};
