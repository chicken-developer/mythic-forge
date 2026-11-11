#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Models/GolemRPGModels.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "GolemRPGComponent.generated.h"

class AGolemItemPickup;
class UGolemDefinition;
class UGolemInventoryComponent;
class UGolemItemDefinition;
class UGolemSkillDefinition;
class UGolemSkillTreeDefinition;
class UGolemRPGComponent;

USTRUCT()
struct GOLEMITEMS_API FGolemRPGRecord : public FFastArraySerializerItem
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
	FGolemRPGGolemView Golem;
};

USTRUCT()
struct GOLEMITEMS_API FGolemRPGArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGolemRPGRecord> Records;
	UPROPERTY(NotReplicated)
	TWeakObjectPtr<UGolemRPGComponent> Owner;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams);
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);
};

template <>
struct TStructOpsTypeTraits<FGolemRPGArray> : public TStructOpsTypeTraitsBase2<FGolemRPGArray>
{
	enum
	{
		WithNetDeltaSerializer = true
	};
};

DECLARE_MULTICAST_DELEGATE(FOnGolemRPGSnapshotChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemRPGCommandCompleted, const FGolemRPGCommandResult&);

/** Shared authority coordinates inventory values and three independent source-owned GAS projections. */
UCLASS(ClassGroup = (Golem), meta = (BlueprintSpawnableComponent))
class GOLEMITEMS_API UGolemRPGComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGolemRPGComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGolemResult InitializeRPG(UGolemInventoryComponent* InInventory, const FPrimaryAssetId& InSkillTreeId,
		TConstArrayView<FGolemRPGGolemRegistration> InRegistrations, int32 InitialPoints = 2);
	/** Clients bind the replicated sibling without granting or mutating anything. */
	FGolemResult BindReplicatedInventory(UGolemInventoryComponent* InInventory);
	FGolemResult InitializeFixtureCatalog(TConstArrayView<UGolemDefinition*> Definitions);
	/** SkillLab-only unlock seed, configured once before RPG initialization. Legacy maps keep empty builds. */
	FGolemResult ConfigureSkillLabBuilds(const TMap<FPrimaryAssetId, FGolemSkillBuild>& InBuilds);
	FGolemResult ConfigureGearDemo(const FGuid& DemoRunId, int64 InitialCurrency, int32 InitialLevel,
		bool bEquipStarter = true);
	FGolemResult RegisterRewardBundle(const FGolemGearRewardBundle& Bundle);
	TArray<FGolemGearRewardBundle> GetRewardOffers() const;
	FGolemResult PreviewEquip(const FGolemEntityId& GolemId, const FGuid& InstanceId,
		FGameplayTag Slot, FGolemItemStatBonuses& OutStatBonuses) const;
	FGolemResult PreviewTransfer(const FGolemEntityId& SourceGolemId, const FGolemEntityId& TargetGolemId,
		const FGuid& InstanceId, FGameplayTag SlotTag, FGolemItemStatBonuses& OutSource,
		FGolemItemStatBonuses& OutTarget) const;
	FGuid GetWeaponRuneParentId(const FGuid& RuneId) const;
	/** Read an exact same-human receipt before fresh routing; NotFound means no committed request exists. */
	FGolemRPGCommandResult QueryCommittedCommand(const FGolemRPGCallerContext& Caller,
		const FGolemRPGCommand& Command) const;
	FGolemRPGCommandResult ExecuteCommand(const FGolemRPGCallerContext& Caller, const FGolemRPGCommand& Command);
	FGolemResult RegisterPickup(AGolemItemPickup* Pickup);
	/** Composition may withdraw its exact unclaimed actor after a failed reward spawn batch. */
	FGolemResult UnregisterUnclaimedPickup(AGolemItemPickup* Pickup);
	FGolemRPGView GetView() const;
	FGolemRPGGolemView GetGolemView(const FGolemEntityId& GolemId) const;
	bool IsInitialized() const;
	bool IsTransactionInProgress() const;
	FGolemResult CaptureSnapshot(FGolemRPGSnapshot& OutSnapshot) const;
	/** Fresh inventory/coordinator only; journal reconstruction and all projections validate before commit. */
	FGolemResult TryRestoreSnapshot(UGolemInventoryComponent* InInventory, const FGolemRPGSnapshot& Snapshot,
		TConstArrayView<FGolemRPGGolemRegistration> InRegistrations);
	/** Explicit legacy-to-twelve-slot import; preserves journals/GUIDs and grants no fixture rewards or currency. */
	FGolemResult TryRestoreItemizationSnapshot(UGolemInventoryComponent* InInventory,
		const FGolemRPGSnapshot& Snapshot, TConstArrayView<FGolemRPGGolemRegistration> InRegistrations,
		const FGuid& MigrationId);
	FOnGolemRPGSnapshotChanged OnSnapshotChanged;
	FOnGolemRPGCommandCompleted OnCommandCompleted;

protected:
	virtual void OnRegister() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend struct FGolemRPGArray;
	FGolemResult ValidateAuthority() const;
	FGolemResult ValidateCaller(const FGolemRPGCallerContext& Caller) const;
	FGolemResult ValidateRegistrations(TConstArrayView<FGolemRPGGolemRegistration> InRegistrations,
		const FGolemRPGView* ExpectedView = nullptr, bool bAllowMutationFence = false,
		bool bRequireFreshSource = true) const;
	FGolemResult ResolveDefinition(const FPrimaryAssetId& Id, UClass* ExpectedClass,
		const UGolemDefinition*& OutDefinition) const;
	FGolemResult ResolveSkills(const FPrimaryAssetId& TreeId);
	FGolemResult BuildCandidate(const FGolemRPGView& Existing, const FGolemInventoryView& InventoryView,
		const FGolemRPGCommand& Command, const FGolemItemInstance* PickedItem,
		const TArray<FGolemInventoryMutation>* HistoricalMutations, FGolemRPGView& OutView,
		FGolemInventoryRequest& OutInventoryRequest, FGuid& OutAffectedInstance) const;
	FGolemResult BuildProjection(FGolemRPGGolemView& Golem, const TArray<FGolemItemInstance>& Items,
		FGolemGASProjectionPlan& OutPlan) const;
	FGolemResult ValidateSnapshot(const FGolemRPGSnapshot& Snapshot, FGolemRPGView& OutView) const;
	FGolemResult RestoreSnapshot(UGolemInventoryComponent* InInventory, const FGolemRPGSnapshot& Snapshot,
		TConstArrayView<FGolemRPGGolemRegistration> InRegistrations, const FGuid& MigrationId);
	FGolemResult BuildItemizationMigration(const FGolemRPGView& Existing,
		const FGolemInventoryView& InventoryView, const FGuid& MigrationId,
		FGolemRPGView& OutView, FGolemInventoryRequest& OutRequest) const;
	FGolemResult AppendItemizationMigration(FGolemRPGSnapshot& Snapshot, const FGuid& MigrationId) const;
	FGolemResult ValidateBuildAttachments(const FGolemRPGGolemView& Golem,
		const FGolemRPGView& View, const TArray<FGolemItemInstance>& Items) const;
	FGolemResult BuildSkillCandidate(FGolemRPGGolemView& Golem, const FGolemRPGView& Existing,
		const FGolemInventoryView& InventoryView, const FGolemRPGCommand& Command) const;
	FGolemResult BuildGearCandidate(FGolemRPGView& Candidate, const FGolemInventoryView& InventoryView,
		const FGolemRPGCommand& Command, const TArray<FGolemInventoryMutation>* HistoricalMutations,
		FGolemInventoryRequest& Request, FGuid& OutAffected) const;
	FGolemRPGCommandResult ExecuteGearCommand(const FGolemRPGCallerContext& Caller, const FGolemRPGCommand& Command);
	FGolemResult SynchronizeLocations(const FGolemRPGView& Candidate, const FGolemInventoryView& InventoryView,
		FGolemInventoryRequest& Request) const;
	FGolemResult SeedGearAssignments(FGolemRPGView& Candidate, const FGolemInventoryView& InventoryView,
		FGolemInventoryRequest& Request) const;
	FGolemResult ValidateGearLocations(const FGolemRPGView& Candidate, const TArray<FGolemItemInstance>& Items) const;
	static FGolemResult MigrateSnapshot(FGolemRPGSnapshot& Snapshot);
	void RefreshReplicatedState();
	void HandleReplicatedStateReady();
	void HandleInventoryChanged();
	void BindInventory(UGolemInventoryComponent* InInventory);
	static void SortView(FGolemRPGView& View);

	UPROPERTY(Replicated)
	FGolemRPGArray ReplicatedState;
	UPROPERTY(Transient)
	TObjectPtr<UGolemInventoryComponent> Inventory;
	UPROPERTY(Transient)
	TMap<FPrimaryAssetId, TObjectPtr<UGolemDefinition>> FixtureDefinitions;
	UPROPERTY(Transient)
	TMap<FPrimaryAssetId, TObjectPtr<UGolemSkillDefinition>> Skills;
	UPROPERTY(Transient)
	TMap<FPrimaryAssetId, FGolemSkillBuild> InitialSkillLabBuilds;
	UPROPERTY(Transient)
	TArray<FGolemGearRewardBundle> RewardOffers;
	TMap<FGolemEntityId, FGolemRPGGolemRegistration> Registrations;
	TMap<FGuid, TWeakObjectPtr<AGolemItemPickup>> Pickups;
	TArray<FGolemRPGReceipt> Receipts;
	TMap<FGuid, int32> ReceiptIndices;
	FGolemRPGView PublishedView;
	FGolemRPGView InitialView;
	FGolemInventorySnapshot InitialInventory;
	FGolemItemizationMigration ItemizationMigration;
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
