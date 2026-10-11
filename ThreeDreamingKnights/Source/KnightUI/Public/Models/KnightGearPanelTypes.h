#pragma once

#include "CoreMinimal.h"
#include "Types/KnightStableIds.h"
#include "UObject/PrimaryAssetId.h"

enum class EKnightGearPanelAction : uint8
{
	SelectKnight,
	Equip,
	Unequip,
	Transfer,
	SwapRings,
	AttachWeaponRune,
	DetachWeaponRune,
	AttachSkillRune,
	DetachSkillRune,
	Upgrade,
	Buy,
	Sell,
	BindConsumable,
	Fixture
};

/** An exact, revision-bound intent. All costs and effects are recomputed by the existing item domain. */
struct KNIGHTUI_API FKnightGearPanelIntent
{
	EKnightGearPanelAction Action = EKnightGearPanelAction::Equip;
	FKnightEntityId KnightId;
	FKnightEntityId TargetKnightId;
	FGuid RequestId;
	FGuid InstanceId;
	FGuid RuneInstanceId;
	FGuid RewardId;
	FPrimaryAssetId DefinitionId;
	FName SlotId;
	FName FixtureAction;
	int64 ExpectedInventoryRevision = 0;
	int64 ExpectedKnightRevision = 0;
	int64 ExpectedTargetKnightRevision = 0;
	int64 ExpectedItemRevision = 0;
	int32 Quantity = 1;
	int32 QuickSlot = 0;
	bool bConfirmEpicSale = false;
};

struct KNIGHTUI_API FKnightGearItemRow
{
	FGuid InstanceId;
	FPrimaryAssetId DefinitionId;
	FKnightEntityId AssignedKnightId;
	FName Category;
	FName Subtype;
	FName Rarity;
	FName LocationId;
	FName SlotId;
	FName RuneId;
	FGuid AttachedRuneId;
	int64 ItemRevision = 0;
	int32 Count = 0;
	int32 ItemLevel = 0;
	int32 RequiredLevel = 1;
	int32 UpgradeRank = 0;
	int32 BuyPrice = 0;
	int32 SellPrice = 0;
	FText Name;
	FText Badge;
	FText Location;
	FText Requirement;
	FText Tooltip;
	FLinearColor QualityColor = FLinearColor::White;
	bool bUsable = false;
	bool bCanUpgrade = false;
	bool bVendorStock = false;
};

struct KNIGHTUI_API FKnightGearSlotRow
{
	FName SlotId;
	FText Name;
	FGuid InstanceId;
	FText ItemName;
	FText Badge;
	FText Tooltip;
	FLinearColor QualityColor = FLinearColor::White;
	bool bLocked = false;
};

struct KNIGHTUI_API FKnightGearKnightRow
{
	FKnightEntityId KnightId;
	FText Name;
	int32 Level = 1;
	int64 Revision = 0;
	bool bCanEdit = false;
};

struct KNIGHTUI_API FKnightGearQuickUseRow
{
	FPrimaryAssetId DefinitionId;
	FText Name;
	int32 Count = 0;
	float CooldownEndTime = 0.0f;
};

struct KNIGHTUI_API FKnightGearStatRow
{
	FText Name;
	FText Before;
	FText After;
	FText Contribution;
};

/** Pure proposed totals. No temporary effects are applied to any live Knight to produce this view. */
struct KNIGHTUI_API FKnightGearCompare
{
	FGuid InstanceId;
	FKnightEntityId KnightId;
	FName SlotId;
	TArray<FKnightGearStatRow> Stats;
	FText Summary;
	FText RejectionReason;
	bool bValid = false;
};

struct KNIGHTUI_API FKnightGearPanelSnapshot
{
	bool bEnabled = false;
	bool bCanEdit = false;
	bool bCanUseVendor = false;
	bool bCanUpgrade = false;
	bool bCanRunFixtures = false;
	bool bHasDemoState = false;
	bool bCatalogLab = false;
	FGuid ClaimableRewardId;
	FGuid SkillRuneInstanceId;
	FKnightEntityId KnightId;
	int32 Level = 1;
	int64 Currency = 0;
	int64 InventoryRevision = 0;
	int64 KnightRevision = 0;
	int32 RuntimeRevision = 0;
	FText KnightName;
	FText EditLockReason;
	FText FixtureSummary;
	FText RelicStatus;
	FText Attachments;
	TArray<FKnightGearKnightRow> Knights;
	TArray<FKnightGearSlotRow> Slots;
	TArray<FKnightGearItemRow> Items;
	TArray<FKnightGearItemRow> VendorStock;
	TArray<FKnightGearQuickUseRow> QuickUses;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightGearPanelCommand, const FKnightGearPanelIntent&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightGearPanelPreview, const FKnightGearPanelIntent&);
