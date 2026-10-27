#pragma once

#include "CoreMinimal.h"
#include "Commands/KnightRPGCommand.h"
#include "Models/KnightItemModels.h"
#include "Models/KnightGearModels.h"
#include "Projection/KnightSourceProjection.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "KnightRPGModels.generated.h"

class UKnightAbilitySystemComponent;

namespace KnightRPG
{
	inline constexpr int32 SnapshotVersion = 3;
	inline constexpr int32 MaximumTalentLoadouts = 16;
	inline constexpr int32 MaximumLoadoutNameLength = 48;
	inline constexpr int32 MaximumSkills = 64;
	inline constexpr int32 MaximumSkillPoints = 100000;
	inline constexpr int32 MaximumLootEntries = 32;
	inline constexpr float PickupRange = 300.0f;
	inline const FName& ProjectionSource = KnightProjection::RPGSourceKey;
}

/** Computed by the composition after canonical human ownership validation. Never sent by a client. */
struct KNIGHTITEMS_API FKnightRPGCallerContext
{
	FGuid HumanId;
	FKnightEntityId KnightId;
	TWeakObjectPtr<AActor> Pawn;
	TWeakObjectPtr<UKnightAbilitySystemComponent> AbilitySystem;
	FPrimaryAssetId HeroDefinitionId;
	bool bInSafeZone = false;
	bool bInCombat = false;
	bool bQuestDeliveryAuthorized = false;
	FGuid EncounterId;
	/** Server composition fills this from current ownership, never from client payloads. */
	TArray<FKnightEntityId> EditableKnights;
};

struct KNIGHTITEMS_API FKnightRPGKnightRegistration
{
	FKnightEntityId KnightId;
	FPrimaryAssetId HeroDefinitionId;
	TWeakObjectPtr<AActor> Pawn;
	TWeakObjectPtr<UKnightAbilitySystemComponent> AbilitySystem;
};

USTRUCT(BlueprintType)
struct KNIGHTITEMS_API FKnightRuneTuning
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Rune")
	float DamageScale = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Rune")
	float CostScale = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Rune")
	float DurationScale = 1.0f;

	FKnightResult Validate() const;
	bool operator==(const FKnightRuneTuning& Other) const;
};

USTRUCT(BlueprintType)
struct KNIGHTITEMS_API FKnightMaterialCost
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Skills")
	FPrimaryAssetId DefinitionId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Skills")
	int32 Count = 0;

	bool operator==(const FKnightMaterialCost& Other) const;
};

USTRUCT(BlueprintType)
struct KNIGHTITEMS_API FKnightEquipmentAssignment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Equipment")
	FGameplayTag SlotTag;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Equipment")
	FGuid ItemInstanceId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Equipment")
	FPrimaryAssetId DefinitionId;

	bool operator==(const FKnightEquipmentAssignment& Other) const;
};

/** Refunds use these exact paid amounts, including the identities of retired stacks. */
USTRUCT(BlueprintType)
struct KNIGHTITEMS_API FKnightPurchasedSkill
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Skills")
	FPrimaryAssetId DefinitionId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Skills")
	int32 PaidPoints = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Skills")
	TArray<FKnightItemInstance> PaidMaterials;

	bool operator==(const FKnightPurchasedSkill& Other) const;
};

USTRUCT(BlueprintType)
struct KNIGHTITEMS_API FKnightTalentLoadout
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Talents")
	FGuid LoadoutId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Talents")
	FString Name;
	/** Talent presets contain spec and choices only; equipment and rune attachments remain independent. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Talents")
	FKnightSkillBuild Build;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Talents")
	int32 Version = 1;

	bool operator==(const FKnightTalentLoadout& Other) const;
};

USTRUCT(BlueprintType)
struct KNIGHTITEMS_API FKnightRPGKnightView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Identity")
	FKnightEntityId KnightId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Identity")
	FPrimaryAssetId HeroDefinitionId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Transactions")
	int64 Revision = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Skills")
	int32 SkillPoints = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Equipment")
	TArray<FKnightEquipmentAssignment> Assignments;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Skills")
	TArray<FKnightPurchasedSkill> PurchasedSkills;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Stats")
	FKnightItemStatBonuses StatBonuses;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Rune")
	FKnightRuneTuning RuneTuning;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Talents")
	FKnightSkillBuild Build;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Rune")
	FGuid WeaponRuneInstanceId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Rune")
	FGuid SkillRuneInstanceId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Talents")
	TArray<FKnightTalentLoadout> TalentLoadouts;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Talents")
	FGuid ActiveTalentLoadoutId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Gear")
	bool bGearEnabled = false;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Progression")
	int32 Level = 1;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Consumables")
	TArray<FPrimaryAssetId> ConsumableBindings;

	bool IsInitialized() const;
	const FKnightEquipmentAssignment* FindAssignment(FGameplayTag SlotTag) const;
	bool HasSkill(const FPrimaryAssetId& SkillId) const;
	bool operator==(const FKnightRPGKnightView& Other) const;
};

USTRUCT(BlueprintType)
struct KNIGHTITEMS_API FKnightRPGView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Transactions")
	int64 Revision = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Inventory")
	FGuid InventoryId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Inventory")
	int64 InventoryRevision = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Skills")
	FPrimaryAssetId SkillTreeId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Identity")
	TArray<FKnightRPGKnightView> Knights;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Currency")
	int64 Currency = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Demo")
	FGuid DemoRunId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Demo")
	bool bGearDemo = false;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Knight|RPG|Rewards")
	TArray<FGuid> ClaimedRewards;

	bool IsInitialized() const;
	/** Twelve-slot authority is independent of isolated demo rewards and fixture presets. */
	bool IsItemizationEnabled() const;
	bool operator==(const FKnightRPGView& Other) const;
};

/** Historical facts are durable values; no actors, capabilities, specs or GE handles are persisted. */
USTRUCT()
struct KNIGHTITEMS_API FKnightRPGReceipt
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGuid HumanId;
	UPROPERTY(SaveGame)
	FKnightRPGCommand Command;
	UPROPERTY(SaveGame)
	FKnightInventoryRequest InventoryRequest;
	UPROPERTY(SaveGame)
	FKnightItemInstance PickedItem;
	UPROPERTY(SaveGame)
	FKnightRPGKnightView CommittedKnight;
	UPROPERTY(SaveGame)
	int64 CommittedInventoryRevision = 0;
	UPROPERTY(SaveGame)
	FGuid AffectedInstanceId;
	UPROPERTY(SaveGame)
	TArray<FKnightRPGKnightView> CommittedKnights;
	UPROPERTY(SaveGame)
	int64 CommittedCurrency = 0;
	UPROPERTY(SaveGame)
	FKnightGearRewardBundle Reward;

	bool operator==(const FKnightRPGReceipt& Other) const;
};

USTRUCT()
struct KNIGHTITEMS_API FKnightRPGCombatSnapshot
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FKnightSkillRuntimeSnapshot Skills;
	UPROPERTY(SaveGame)
	float Health = 0.0f;
	UPROPERTY(SaveGame)
	float Stamina = 0.0f;

	bool operator==(const FKnightRPGCombatSnapshot& Other) const;
};

/** One deterministic administrative boundary after the immutable historical request prefix. */
USTRUCT()
struct KNIGHTITEMS_API FKnightItemizationMigration
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGuid MigrationId;
	UPROPERTY(SaveGame)
	int32 LegacyReceiptCount = INDEX_NONE;
	UPROPERTY(SaveGame)
	FKnightInventoryRequest InventoryRequest;

	bool IsPresent() const;
	bool operator==(const FKnightItemizationMigration& Other) const;
};

USTRUCT()
struct KNIGHTITEMS_API FKnightRPGSnapshot
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 SchemaVersion = KnightRPG::SnapshotVersion;
	UPROPERTY(SaveGame)
	int32 InitialSkillPoints = 2;
	UPROPERTY(SaveGame)
	FKnightRPGView InitialView;
	UPROPERTY(SaveGame)
	FKnightInventorySnapshot InitialInventory;
	UPROPERTY(SaveGame)
	FKnightRPGView View;
	UPROPERTY(SaveGame)
	FKnightInventorySnapshot Inventory;
	UPROPERTY(SaveGame)
	TArray<FKnightRPGReceipt> Receipts;
	UPROPERTY(SaveGame)
	TMap<FKnightEntityId, FKnightRPGCombatSnapshot> CombatStates;
	UPROPERTY(SaveGame)
	TArray<FKnightGearRewardBundle> RewardOffers;
	/** Schema adapters preserve old inventory receipts and derive exact parent ownership separately. */
	UPROPERTY(SaveGame)
	TMap<FGuid, FGuid> LegacyWeaponRuneParents;
	UPROPERTY(SaveGame)
	FKnightItemizationMigration ItemizationMigration;

	bool operator==(const FKnightRPGSnapshot& Other) const;
};
