#pragma once

#include "CoreMinimal.h"
#include "Types/GolemStableIds.h"
#include "UObject/PrimaryAssetId.h"

enum class EGolemGearPanelAction : uint8
{
	SelectGolem,
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
struct GOLEMUI_API FGolemGearPanelIntent
{
	EGolemGearPanelAction Action = EGolemGearPanelAction::Equip;
	FGolemEntityId GolemId;
	FGolemEntityId TargetGolemId;
	FGuid RequestId;
	FGuid InstanceId;
	FGuid RuneInstanceId;
	FGuid RewardId;
	FPrimaryAssetId DefinitionId;
	FName SlotId;
	FName FixtureAction;
	int64 ExpectedInventoryRevision = 0;
	int64 ExpectedGolemRevision = 0;
	int64 ExpectedTargetGolemRevision = 0;
	int64 ExpectedItemRevision = 0;
	int32 Quantity = 1;
	int32 QuickSlot = 0;
	bool bConfirmEpicSale = false;
};

struct GOLEMUI_API FGolemGearItemRow
{
	FGuid InstanceId;
	FPrimaryAssetId DefinitionId;
	FGolemEntityId AssignedGolemId;
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

struct GOLEMUI_API FGolemGearSlotRow
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

struct GOLEMUI_API FGolemGearGolemRow
{
	FGolemEntityId GolemId;
	FText Name;
	int32 Level = 1;
	int64 Revision = 0;
	bool bCanEdit = false;
};

struct GOLEMUI_API FGolemGearQuickUseRow
{
	FPrimaryAssetId DefinitionId;
	FText Name;
	int32 Count = 0;
	float CooldownEndTime = 0.0f;
};

struct GOLEMUI_API FGolemGearStatRow
{
	FText Name;
	FText Before;
	FText After;
	FText Contribution;
};

/** Pure proposed totals. No temporary effects are applied to any live Golem to produce this view. */
struct GOLEMUI_API FGolemGearCompare
{
	FGuid InstanceId;
	FGolemEntityId GolemId;
	FName SlotId;
	TArray<FGolemGearStatRow> Stats;
	FText Summary;
	FText RejectionReason;
	bool bValid = false;
};

struct GOLEMUI_API FGolemGearPanelSnapshot
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
	FGolemEntityId GolemId;
	int32 Level = 1;
	int64 Currency = 0;
	int64 InventoryRevision = 0;
	int64 GolemRevision = 0;
	int32 RuntimeRevision = 0;
	FText GolemName;
	FText EditLockReason;
	FText FixtureSummary;
	FText RelicStatus;
	FText Attachments;
	TArray<FGolemGearGolemRow> Golems;
	TArray<FGolemGearSlotRow> Slots;
	TArray<FGolemGearItemRow> Items;
	TArray<FGolemGearItemRow> VendorStock;
	TArray<FGolemGearQuickUseRow> QuickUses;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemGearPanelCommand, const FGolemGearPanelIntent&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemGearPanelPreview, const FGolemGearPanelIntent&);
