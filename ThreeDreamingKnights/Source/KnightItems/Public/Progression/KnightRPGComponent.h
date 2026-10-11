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
	int64 Currency = 0;
	UPROPERTY()
	FGuid DemoRunId;
	UPROPERTY()
	bool bGearDemo = false;
	UPROPERTY()
	TArray<FGuid> ClaimedRewards;
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
	/** SkillLab-only unlock seed, configured once before RPG initialization. Legacy maps keep empty builds. */
	FKnightResult ConfigureSkillLabBuilds(const TMap<FPrimaryAssetId, FKnightSkillBuild>& InBuilds);
	FKnightResult ConfigureGearDemo(const FGuid& DemoRunId, int64 InitialCurrency, int32 InitialLevel,
		bool bEquipStarter = true);
	FKnightResult RegisterRewardBundle(const FKnightGearRewardBundle& Bundle);
	TArray<FKnightGearRewardBundle> GetRewardOffers() const;
	FKnightResult PreviewEquip(const FKnightEntityId& KnightId, const FGuid& InstanceId,
		FGameplayTag Slot, FKnightItemStatBonuses& OutStatBonuses) const;
	FKnightResult PreviewTransfer(const FKnightEntityId& SourceKnightId, const FKnightEntityId& TargetKnightId,
		const FGuid& InstanceId, FGameplayTag SlotTag, FKnightItemStatBonuses& OutSource,
		FKnightItemStatBonuses& OutTarget) const;
	FGuid GetWeaponRuneParentId(const FGuid& RuneId) const;
	/** Read an exact same-human receipt before fresh routing; NotFound means no committed request exists. */
	FKnightRPGCommandResult QueryCommittedCommand(const FKnightRPGCallerContext& Caller,
		const FKnightRPGCommand& Command) const;
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
	/** Explicit legacy-to-twelve-slot import; preserves journals/GUIDs and grants no fixture rewards or currency. */
	FKnightResult TryRestoreItemizationSnapshot(UKnightInventoryComponent* InInventory,
		const FKnightRPGSnapshot& Snapshot, TConstArrayView<FKnightRPGKnightRegistration> InRegistrations,
		const FGuid& MigrationId);
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
	FKnightResult RestoreSnapshot(UKnightInventoryComponent* InInventory, const FKnightRPGSnapshot& Snapshot,
		TConstArrayView<FKnightRPGKnightRegistration> InRegistrations, const FGuid& MigrationId);
	FKnightResult BuildItemizationMigration(const FKnightRPGView& Existing,
		const FKnightInventoryView& InventoryView, const FGuid& MigrationId,
		FKnightRPGView& OutView, FKnightInventoryRequest& OutRequest) const;
	FKnightResult AppendItemizationMigration(FKnightRPGSnapshot& Snapshot, const FGuid& MigrationId) const;
	FKnightResult ValidateBuildAttachments(const FKnightRPGKnightView& Knight,
		const FKnightRPGView& View, const TArray<FKnightItemInstance>& Items) const;
	FKnightResult BuildSkillCandidate(FKnightRPGKnightView& Knight, const FKnightRPGView& Existing,
		const FKnightInventoryView& InventoryView, const FKnightRPGCommand& Command) const;
	FKnightResult BuildGearCandidate(FKnightRPGView& Candidate, const FKnightInventoryView& InventoryView,
		const FKnightRPGCommand& Command, const TArray<FKnightInventoryMutation>* HistoricalMutations,
		FKnightInventoryRequest& Request, FGuid& OutAffected) const;
	FKnightRPGCommandResult ExecuteGearCommand(const FKnightRPGCallerContext& Caller, const FKnightRPGCommand& Command);
	FKnightResult SynchronizeLocations(const FKnightRPGView& Candidate, const FKnightInventoryView& InventoryView,
		FKnightInventoryRequest& Request) const;
	FKnightResult SeedGearAssignments(FKnightRPGView& Candidate, const FKnightInventoryView& InventoryView,
		FKnightInventoryRequest& Request) const;
	FKnightResult ValidateGearLocations(const FKnightRPGView& Candidate, const TArray<FKnightItemInstance>& Items) const;
	static FKnightResult MigrateSnapshot(FKnightRPGSnapshot& Snapshot);
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
	UPROPERTY(Transient)
	TMap<FPrimaryAssetId, FKnightSkillBuild> InitialSkillLabBuilds;
	UPROPERTY(Transient)
	TArray<FKnightGearRewardBundle> RewardOffers;
	TMap<FKnightEntityId, FKnightRPGKnightRegistration> Registrations;
	TMap<FGuid, TWeakObjectPtr<AKnightItemPickup>> Pickups;
	TArray<FKnightRPGReceipt> Receipts;
	TMap<FGuid, int32> ReceiptIndices;
	FKnightRPGView PublishedView;
	FKnightRPGView InitialView;
	FKnightInventorySnapshot InitialInventory;
	FKnightItemizationMigration ItemizationMigration;
	FDelegateHandle InventoryChangedHandle;
	int32 InitialSkillPoints = 2;
	FGuid GearDemoRunId;
	int64 GearInitialCurrency = 0;
	int32 GearInitialLevel = 1;
	bool bGearEquipStarter = false;
	bool bHasFixtureCatalog = false;
	bool bTransactionInProgress = false;
	bool bEndingPlay = false;
};
