#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/GolemResult.h"
#include "Types/GolemStableIds.h"
#include "UObject/PrimaryAssetId.h"
#include "GolemItemModels.generated.h"

namespace GolemItems
{
	inline constexpr int32 MaximumStackCount = 10000;
	inline constexpr int32 MaximumInventoryCapacity = 256;
	inline constexpr int32 MaximumBatchOperations = 256;
	/** Every bag hammer may own a non-bag rune, plus three twelve-slot and two-socket loadouts. */
	inline constexpr int32 MaximumOwnedInstances = MaximumInventoryCapacity * 2 + 42;
	/** The complete successful journal is retained; saturation rejects writes instead of allowing old replay. */
	inline constexpr int32 MaximumTransactionReceipts = 1024;
	inline constexpr int32 InventorySnapshotVersion = 2;
	inline constexpr float MaximumStatBonus = 10000.0f;
}

UENUM(BlueprintType)
enum class EGolemItemKind : uint8
{
	Material,
	Consumable,
	Equipment,
	Rune,
	QuestItem,
	Junk
};

/** Additive tuning data only; applying/revoking GAS effects belongs to the equipment composition. */
USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemItemStatBonuses
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|Items|Stats")
	float MaxHealth = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|Items|Stats")
	float MaxStamina = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|Items|Stats")
	float MaxRuneEnergy = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|Items|Stats")
	float AttackPower = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|Items|Stats")
	float Defense = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|Items|Stats")
	float Poise = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|Items|Stats")
	float CritChance = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|Items|Stats")
	float Haste = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|Items|Stats")
	float CooldownRecovery = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|Items|Stats")
	float MasteryBonus = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|Items|Stats")
	float Versatility = 0.0f;

	FGolemResult Validate() const;
	void Add(const FGolemItemStatBonuses& Other);
	bool IsEmpty() const;
	bool operator==(const FGolemItemStatBonuses& Other) const;
};

/** Persistent runtime instance: no asset, actor or UObject addresses enter inventory snapshots. */
USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemItemInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Items|Identity")
	FGuid InstanceId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Items|Identity")
	FPrimaryAssetId DefinitionId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Items|Inventory")
	int32 Count = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Items|Identity")
	int64 ItemRevision = 1;
	/** Stable names are persisted instead of location enum ordinals. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Items|Location")
	FName LocationId = TEXT("Inventory");
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Items|Location")
	FGolemEntityId AssignedGolemId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Items|Location")
	FGameplayTag SlotTag;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Items|Upgrade")
	int32 UpgradeRank = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Items|Rune")
	FGuid RuneAttachmentId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Items|Rune")
	FGuid ParentWeaponId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|Items|Source")
	FGuid RewardReceiptId;

	FGolemResult ValidateIdentityAndCount() const;
	bool operator==(const FGolemItemInstance& Other) const;
};

UENUM()
enum class EGolemInventoryOperation : uint8
{
	Add,
	Remove,
	Update
};

/** Count is the amount to add/remove. Removing also requires the exact expected definition identity. */
USTRUCT()
struct GOLEMITEMS_API FGolemInventoryMutation
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	EGolemInventoryOperation Operation = EGolemInventoryOperation::Add;

	UPROPERTY(SaveGame)
	FGolemItemInstance Item;
	UPROPERTY(SaveGame)
	int64 ExpectedItemRevision = 0;

	bool operator==(const FGolemInventoryMutation& Other) const;
};

/** Native internal command; caller ownership must be validated by the authoritative composition before calling. */
USTRUCT()
struct GOLEMITEMS_API FGolemInventoryRequest
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGuid RequestId;

	UPROPERTY(SaveGame)
	int64 ExpectedRevision = 0;

	UPROPERTY(SaveGame)
	TArray<FGolemInventoryMutation> Mutations;

	FGolemResult ValidateShape() const;
	bool operator==(const FGolemInventoryRequest& Other) const;
};

USTRUCT()
struct GOLEMITEMS_API FGolemInventoryReceipt
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGolemInventoryRequest Request;

	UPROPERTY(SaveGame)
	int64 CommittedRevision = 0;

	bool operator==(const FGolemInventoryReceipt& Other) const;
};

USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemInventoryTransactionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Items|Transactions")
	FGolemResult Result;

	/** Revision of this request's commit, including an exact replay after subsequent transactions. */
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Items|Transactions")
	int64 CommittedRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Items|Transactions")
	bool bWasReplay = false;
};

/** Initial read followed by OnSnapshotChanged; revisions are published only when all Fast Array records converge. */
USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemInventoryView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Items|Inventory")
	FGuid InventoryId;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Items|Inventory")
	int32 Capacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Items|Inventory")
	int64 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Golem|Items|Inventory")
	TArray<FGolemItemInstance> Items;

	bool IsInitialized() const;
	int32 GetBagCount() const;
	bool operator==(const FGolemInventoryView& Other) const;
};

/** Authority-only persistence fragment. Replay the complete bounded journal to verify the claimed final state. */
USTRUCT()
struct GOLEMITEMS_API FGolemInventorySnapshot
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 SchemaVersion = GolemItems::InventorySnapshotVersion;

	UPROPERTY(SaveGame)
	FGuid InventoryId;

	UPROPERTY(SaveGame)
	int32 Capacity = 0;

	UPROPERTY(SaveGame)
	int64 Revision = 0;

	UPROPERTY(SaveGame)
	TArray<FGolemItemInstance> Items;

	UPROPERTY(SaveGame)
	TArray<FGolemInventoryReceipt> Receipts;

	bool operator==(const FGolemInventorySnapshot& Other) const;
};
