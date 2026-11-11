#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Skills/GolemSkillTypes.h"
#include "Types/GolemResult.h"
#include "Types/GolemStableIds.h"
#include "GolemRPGCommand.generated.h"

UENUM(BlueprintType)
enum class EGolemRPGOperation : uint8
{
	Pickup,
	Equip,
	Unequip,
	Rune,
	PurchaseSkill,
	Respec,
	ApplyBuild,
	NewTalentLoadout,
	RenameTalentLoadout,
	DuplicateTalentLoadout,
	DeleteTalentLoadout,
	ApplyTalentLoadout,
	TransferEquipment,
	SwapRings,
	AttachWeaponRune,
	DetachWeaponRune,
	UseConsumable,
	BuyItem,
	SellItem,
	UpgradeItem,
	ClaimReward,
	BindConsumable,
	ApplyGearPreset,
	DeliverQuestItem
};

/** Sparse client intent. Costs, counts, definitions and GAS classes are resolved by authority. */
USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemRPGCommand
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	EGolemRPGOperation Operation = EGolemRPGOperation::Pickup;
	UPROPERTY(SaveGame)
	FGuid RequestId;
	UPROPERTY(SaveGame)
	FGolemEntityId ExpectedGolemId;
	UPROPERTY(SaveGame)
	FGuid InventoryId;
	UPROPERTY(SaveGame)
	int64 ExpectedInventoryRevision = 0;
	UPROPERTY(SaveGame)
	int64 ExpectedGolemRevision = 0;
	UPROPERTY(SaveGame)
	FGuid PickupId;
	UPROPERTY(SaveGame)
	FGuid ItemInstanceId;
	UPROPERTY(SaveGame)
	FGameplayTag SlotTag;
	UPROPERTY(SaveGame)
	FPrimaryAssetId SkillDefinitionId;
	UPROPERTY(SaveGame)
	FGolemSkillBuild ProposedBuild;
	UPROPERTY(SaveGame)
	FGuid WeaponRuneInstanceId;
	UPROPERTY(SaveGame)
	FGuid SkillRuneInstanceId;
	UPROPERTY(SaveGame)
	FGuid TalentLoadoutId;
	UPROPERTY(SaveGame)
	FString TalentLoadoutName;
	UPROPERTY(SaveGame)
	FGolemEntityId TargetGolemId;
	UPROPERTY(SaveGame)
	int64 ExpectedTargetGolemRevision = 0;
	UPROPERTY(SaveGame)
	FPrimaryAssetId ItemDefinitionId;
	UPROPERTY(SaveGame)
	int64 ExpectedItemRevision = 0;
	UPROPERTY(SaveGame)
	int32 Quantity = 1;
	UPROPERTY(SaveGame)
	int32 QuickSlot = 0;
	UPROPERTY(SaveGame)
	FGuid RewardId;
	UPROPERTY(SaveGame)
	FName PresetId;
	UPROPERTY(SaveGame)
	bool bConfirmEpicSale = false;

	FGolemResult ValidateShape() const;
	bool operator==(const FGolemRPGCommand& Other) const;
};

USTRUCT(BlueprintType)
struct GOLEMITEMS_API FGolemRPGCommandResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Golem|RPG|Transactions")
	FGolemResult Result;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|RPG|Transactions")
	EGolemRPGOperation Operation = EGolemRPGOperation::Pickup;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|RPG|Transactions")
	FGuid RequestId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|RPG|Transactions")
	FGolemEntityId GolemId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|RPG|Transactions")
	FGuid InventoryId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|RPG|Transactions")
	int64 CommittedInventoryRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|RPG|Transactions")
	int64 CommittedGolemRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|RPG|Transactions")
	FGuid AffectedInstanceId;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|RPG|Transactions")
	bool bWasReplay = false;
};
