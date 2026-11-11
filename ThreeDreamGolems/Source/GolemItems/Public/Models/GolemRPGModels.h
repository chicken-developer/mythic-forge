#pragma once

#include "CoreMinimal.h"
#include "Commands/GolemRPGCommand.h"
#include "Models/GolemItemModels.h"
#include "Models/GolemGearModels.h"
#include "Projection/GolemSourceProjection.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "GolemRPGModels.generated.h"

class UGolemAbilitySystemComponent;

namespace GolemRPG
{
	inline constexpr int32 SnapshotVersion = 3;
	inline constexpr int32 MaximumTalentLoadouts = 16;
	inline constexpr int32 MaximumLoadoutNameLength = 48;
	inline constexpr int32 MaximumSkills = 64;
	inline constexpr int32 MaximumSkillPoints = 100000;
	inline constexpr int32 MaximumLootEntries = 32;
	inline constexpr float PickupRange = 300.0f;
	inline const FName& ProjectionSource = GolemProjection::RPGSourceKey;
}

/** Computed by the composition after canonical human ownership validation. Never sent by a client. */
struct GOLEMITEMS_API FGolemRPGCallerContext
{
	FGuid HumanId;
	FGolemEntityId GolemId;
	TWeakObjectPtr<AActor> Pawn;
	TWeakObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;
	FPrimaryAssetId HeroDefinitionId;
	bool bInSafeZone = false;
	bool bInCombat = false;
	bool bQuestDeliveryAuthorized = false;
	FGuid EncounterId;
	/** Server composition fills this from current ownership, never from client payloads. */
	TArray<FGolemEntityId> EditableGolems;
};

struct GOLEMITEMS_API FGolemRPGGolemRegistration
{
	FGolemEntityId GolemId;
	FPrimaryAssetId HeroDefinitionId;
	TWeakObjectPtr<AActor> Pawn;
	TWeakObjectPtr<UGolemAbilitySystemComponent> AbilitySystem;
};

USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemRuneTuning
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Rune")
	float DamageScale = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Rune")
	float CostScale = 1.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Rune")
	float DurationScale = 1.0f;

	FGolemResult Validate() const;
	bool operator==(const FGolemRuneTuning& Other) const;
};

USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemMaterialCost
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Skills")
	FPrimaryAssetId DefinitionId;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Skills")
	int32 Count = 0;

	bool operator==(const FGolemMaterialCost& Other) const;
};

USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemEquipmentAssignment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Equipment")
	FGameplayTag SlotTag;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Equipment")
	FGuid ItemInstanceId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Equipment")
	FPrimaryAssetId DefinitionId;

	bool operator==(const FGolemEquipmentAssignment& Other) const;
};

/** Refunds use these exact paid amounts, including the identities of retired stacks. */
USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemPurchasedSkill
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Skills")
	FPrimaryAssetId DefinitionId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Skills")
	int32 PaidPoints = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Skills")
	TArray<FGolemItemInstance> PaidMaterials;

	bool operator==(const FGolemPurchasedSkill& Other) const;
};

USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemTalentLoadout
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Talents")
	FGuid LoadoutId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Talents")
	FString Name;
	/** Talent presets contain spec and choices only; equipment and rune attachments remain independent. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Talents")
	FGolemSkillBuild Build;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Talents")
	int32 Version = 1;

	bool operator==(const FGolemTalentLoadout& Other) const;
};

USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemRPGGolemView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Identity")
	FGolemEntityId GolemId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Identity")
	FPrimaryAssetId HeroDefinitionId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Transactions")
	int64 Revision = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Skills")
	int32 SkillPoints = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Equipment")
	TArray<FGolemEquipmentAssignment> Assignments;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Skills")
	TArray<FGolemPurchasedSkill> PurchasedSkills;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Stats")
	FGolemItemStatBonuses StatBonuses;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Rune")
	FGolemRuneTuning RuneTuning;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Talents")
	FGolemSkillBuild Build;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Rune")
	FGuid WeaponRuneInstanceId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Rune")
	FGuid SkillRuneInstanceId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Talents")
	TArray<FGolemTalentLoadout> TalentLoadouts;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Talents")
	FGuid ActiveTalentLoadoutId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Gear")
	bool bGearEnabled = false;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Progression")
	int32 Level = 1;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Consumables")
	TArray<FPrimaryAssetId> ConsumableBindings;

	bool IsInitialized() const;
	const FGolemEquipmentAssignment* FindAssignment(FGameplayTag SlotTag) const;
	bool HasSkill(const FPrimaryAssetId& SkillId) const;
	bool operator==(const FGolemRPGGolemView& Other) const;
};

USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemRPGView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Transactions")
	int64 Revision = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Inventory")
	FGuid InventoryId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Inventory")
	int64 InventoryRevision = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Skills")
	FPrimaryAssetId SkillTreeId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Identity")
	TArray<FGolemRPGGolemView> Golems;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Currency")
	int64 Currency = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Demo")
	FGuid DemoRunId;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Demo")
	bool bGearDemo = false;
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Golem|RPG|Rewards")
	TArray<FGuid> ClaimedRewards;

	bool IsInitialized() const;
	/** Twelve-slot authority is independent of isolated demo rewards and fixture presets. */
	bool IsItemizationEnabled() const;
	bool operator==(const FGolemRPGView& Other) const;
};

/** Historical facts are durable values; no actors, capabilities, specs or GE handles are persisted. */
USTRUCT()
struct GOLEMITEMS_API FGolemRPGReceipt
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGuid HumanId;
	UPROPERTY(SaveGame)
	FGolemRPGCommand Command;
	UPROPERTY(SaveGame)
	FGolemInventoryRequest InventoryRequest;
	UPROPERTY(SaveGame)
	FGolemItemInstance PickedItem;
	UPROPERTY(SaveGame)
	FGolemRPGGolemView CommittedGolem;
	UPROPERTY(SaveGame)
	int64 CommittedInventoryRevision = 0;
	UPROPERTY(SaveGame)
	FGuid AffectedInstanceId;
	UPROPERTY(SaveGame)
	TArray<FGolemRPGGolemView> CommittedGolems;
	UPROPERTY(SaveGame)
	int64 CommittedCurrency = 0;
	UPROPERTY(SaveGame)
	FGolemGearRewardBundle Reward;

	bool operator==(const FGolemRPGReceipt& Other) const;
};

USTRUCT()
struct GOLEMITEMS_API FGolemRPGCombatSnapshot
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGolemSkillRuntimeSnapshot Skills;
	UPROPERTY(SaveGame)
	float Health = 0.0f;
	UPROPERTY(SaveGame)
	float Stamina = 0.0f;

	bool operator==(const FGolemRPGCombatSnapshot& Other) const;
};

/** One deterministic administrative boundary after the immutable historical request prefix. */
USTRUCT()
struct GOLEMITEMS_API FGolemItemizationMigration
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGuid MigrationId;
	UPROPERTY(SaveGame)
	int32 LegacyReceiptCount = INDEX_NONE;
	UPROPERTY(SaveGame)
	FGolemInventoryRequest InventoryRequest;

	bool IsPresent() const;
	bool operator==(const FGolemItemizationMigration& Other) const;
};

USTRUCT()
struct GOLEMITEMS_API FGolemRPGSnapshot
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 SchemaVersion = GolemRPG::SnapshotVersion;
	UPROPERTY(SaveGame)
	int32 InitialSkillPoints = 2;
	UPROPERTY(SaveGame)
	FGolemRPGView InitialView;
	UPROPERTY(SaveGame)
	FGolemInventorySnapshot InitialInventory;
	UPROPERTY(SaveGame)
	FGolemRPGView View;
	UPROPERTY(SaveGame)
	FGolemInventorySnapshot Inventory;
	UPROPERTY(SaveGame)
	TArray<FGolemRPGReceipt> Receipts;
	UPROPERTY(SaveGame)
	TMap<FGolemEntityId, FGolemRPGCombatSnapshot> CombatStates;
	UPROPERTY(SaveGame)
	TArray<FGolemGearRewardBundle> RewardOffers;
	/** Schema adapters preserve old inventory receipts and derive exact parent ownership separately. */
	UPROPERTY(SaveGame)
	TMap<FGuid, FGuid> LegacyWeaponRuneParents;
	UPROPERTY(SaveGame)
	FGolemItemizationMigration ItemizationMigration;

	bool operator==(const FGolemRPGSnapshot& Other) const;
};
