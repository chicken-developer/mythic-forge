#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/KnightResult.h"
#include "Types/KnightStableIds.h"
#include "UObject/PrimaryAssetId.h"
#include "KnightItemModels.generated.h"

namespace KnightItems
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
enum class EKnightItemKind : uint8
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
struct KNIGHTITEMS_API FKnightItemStatBonuses
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|Items|Stats")
	float MaxHealth = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|Items|Stats")
	float MaxStamina = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|Items|Stats")
	float MaxRuneEnergy = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|Items|Stats")
	float AttackPower = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|Items|Stats")
	float Defense = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|Items|Stats")
	float Poise = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|Items|Stats")
	float CritChance = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|Items|Stats")
	float Haste = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|Items|Stats")
	float CooldownRecovery = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|Items|Stats")
	float MasteryBonus = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|Items|Stats")
	float Versatility = 0.0f;

	FKnightResult Validate() const;
	void Add(const FKnightItemStatBonuses& Other);
	bool IsEmpty() const;
	bool operator==(const FKnightItemStatBonuses& Other) const;
};

/** Persistent runtime instance: no asset, actor or UObject addresses enter inventory snapshots. */
USTRUCT(BlueprintType)
struct KNIGHTITEMS_API FKnightItemInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Items|Identity")
	FGuid InstanceId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Items|Identity")
	FPrimaryAssetId DefinitionId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Items|Inventory")
	int32 Count = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Items|Identity")
	int64 ItemRevision = 1;
	/** Stable names are persisted instead of location enum ordinals. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Items|Location")
	FName LocationId = TEXT("Inventory");
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Items|Location")
	FKnightEntityId AssignedKnightId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Items|Location")
	FGameplayTag SlotTag;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Items|Upgrade")
	int32 UpgradeRank = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Items|Rune")
	FGuid RuneAttachmentId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Items|Rune")
	FGuid ParentWeaponId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|Items|Source")
	FGuid RewardReceiptId;

	FKnightResult ValidateIdentityAndCount() const;
	bool operator==(const FKnightItemInstance& Other) const;
};

UENUM()
enum class EKnightInventoryOperation : uint8
{
	Add,
	Remove,
	Update
};

/** Count is the amount to add/remove. Removing also requires the exact expected definition identity. */
USTRUCT()
struct KNIGHTITEMS_API FKnightInventoryMutation
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	EKnightInventoryOperation Operation = EKnightInventoryOperation::Add;

	UPROPERTY(SaveGame)
	FKnightItemInstance Item;
	UPROPERTY(SaveGame)
	int64 ExpectedItemRevision = 0;

	bool operator==(const FKnightInventoryMutation& Other) const;
};

/** Native internal command; caller ownership must be validated by the authoritative composition before calling. */
USTRUCT()
struct KNIGHTITEMS_API FKnightInventoryRequest
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGuid RequestId;

	UPROPERTY(SaveGame)
	int64 ExpectedRevision = 0;

	UPROPERTY(SaveGame)
	TArray<FKnightInventoryMutation> Mutations;

	FKnightResult ValidateShape() const;
	bool operator==(const FKnightInventoryRequest& Other) const;
};

USTRUCT()
struct KNIGHTITEMS_API FKnightInventoryReceipt
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FKnightInventoryRequest Request;

	UPROPERTY(SaveGame)
	int64 CommittedRevision = 0;

	bool operator==(const FKnightInventoryReceipt& Other) const;
};

USTRUCT(BlueprintType)
struct KNIGHTITEMS_API FKnightInventoryTransactionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Items|Transactions")
	FKnightResult Result;

	/** Revision of this request's commit, including an exact replay after subsequent transactions. */
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Items|Transactions")
	int64 CommittedRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Items|Transactions")
	bool bWasReplay = false;
};

/** Initial read followed by OnSnapshotChanged; revisions are published only when all Fast Array records converge. */
USTRUCT(BlueprintType)
struct KNIGHTITEMS_API FKnightInventoryView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Items|Inventory")
	FGuid InventoryId;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Items|Inventory")
	int32 Capacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Items|Inventory")
	int64 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Knight|Items|Inventory")
	TArray<FKnightItemInstance> Items;

	bool IsInitialized() const;
	int32 GetBagCount() const;
	bool operator==(const FKnightInventoryView& Other) const;
};

/** Authority-only persistence fragment. Replay the complete bounded journal to verify the claimed final state. */
USTRUCT()
struct KNIGHTITEMS_API FKnightInventorySnapshot
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 SchemaVersion = KnightItems::InventorySnapshotVersion;

	UPROPERTY(SaveGame)
	FGuid InventoryId;

	UPROPERTY(SaveGame)
	int32 Capacity = 0;

	UPROPERTY(SaveGame)
	int64 Revision = 0;

	UPROPERTY(SaveGame)
	TArray<FKnightItemInstance> Items;

	UPROPERTY(SaveGame)
	TArray<FKnightInventoryReceipt> Receipts;

	bool operator==(const FKnightInventorySnapshot& Other) const;
};
