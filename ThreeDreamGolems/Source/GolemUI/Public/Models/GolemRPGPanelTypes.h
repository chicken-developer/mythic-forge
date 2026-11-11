#pragma once

#include "CoreMinimal.h"
#include "Models/GolemGearPanelTypes.h"
#include "Skills/GolemSkillTypes.h"
#include "Types/GolemStableIds.h"

enum class EGolemRPGPanelAction : uint8
{
	EquipWeapon,
	UnequipWeapon,
	EquipRune,
	UnequipRune,
	PurchaseHammerSkill,
	PurchaseRuneSkill,
	Respec,
	Continue,
	Close,
	ApplyBuild,
	NewTalentLoadout,
	RenameTalentLoadout,
	DuplicateTalentLoadout,
	DeleteTalentLoadout,
	ApplyTalentLoadout,
	SelectGolem
};

enum class EGolemRPGPanelTab : uint8
{
	Character,
	Specialization,
	Talents,
	Spellbook,
	Runes,
	Inventory,
	Campaign,
	Journal,
	Map,
	Settings
};

/** Intent only. The owning controller resolves the domain command and its authority checks. */
struct GOLEMUI_API FGolemRPGPanelCommand
{
	EGolemRPGPanelAction Action = EGolemRPGPanelAction::ApplyBuild;
	FGolemEntityId GolemId;
	int64 ExpectedRevision = 0;
	FGuid RequestId;
	FGolemSkillBuild ProposedBuild;
	FGuid WeaponRuneInstanceId;
	FGuid SkillRuneInstanceId;
	FGuid TalentLoadoutId;
	FString TalentLoadoutName;
};

struct GOLEMUI_API FGolemRPGStatRow
{
	FText Group;
	FText Name;
	FText Value;
	FText Contributions;
};

struct GOLEMUI_API FGolemRPGSpellRow
{
	FName AbilityId;
	FText Name;
	FName Category = TEXT("Spec");
	FText Summary;
	FText Tooltip;
	int32 Slot = 0;
	bool bKnown = true;
	bool bPassive = false;
};

struct GOLEMUI_API FGolemRPGSpecRow
{
	FName SpecId;
	FText Name;
	FText Role;
	FText Summary;
	FText PassiveName;
	FText PassiveTooltip;
	FText MajorBuff;
	FText ChoiceA;
	FText ChoiceB;
	bool bUnlocked = true;
};

struct GOLEMUI_API FGolemRPGRuneRow
{
	FName RuneId;
	FGuid ItemInstanceId;
	FText Name;
	FText Description;
	FText IncompatibilityReason;
	bool bWeaponSocket = false;
	bool bCompatible = false;
};

struct GOLEMUI_API FGolemRPGTalentLoadoutRow
{
	FGuid LoadoutId;
	FString Name;
	FGolemSkillBuild Build;
	int32 Version = 1;
};

struct GOLEMUI_API FGolemRPGGolemChoice
{
	FGolemEntityId GolemId;
	FText Name;
};

/** Pure definition calculation, supplied without applying effects to a live actor. */
struct GOLEMUI_API FGolemRPGBuildPreview
{
	FGolemEntityId GolemId;
	FGolemSkillBuild Build;
	TArray<FGolemRPGSpellRow> Spells;
	TArray<FGolemRPGRuneRow> Runes;
	FText Summary;
};

/** Confirmed state only; populated after domain subscriptions or explicit command responses. */
struct GOLEMUI_API FGolemRPGPanelSnapshot
{
	FGolemGearPanelSnapshot Gear;
	FGolemEntityId GolemId;
	FText Title;
	FText Objective;
	FText Inventory;
	FText Equipment;
	FText WeaponComparison;
	FText RuneComparison;
	FText Skills;
	FText Stats;
	FText Feedback;
	FGolemSkillBuild Build;
	int64 Revision = 0;
	int32 AvailablePoints = 0;
	FGuid WeaponRuneInstanceId;
	FGuid SkillRuneInstanceId;
	FGuid ActiveTalentLoadoutId;
	FGuid LastCompletedRequestId;
	TArray<FGolemRPGGolemChoice> Golems;
	TArray<FGolemRPGStatRow> StatRows;
	TArray<FGolemRPGSpecRow> Specializations;
	TArray<FGolemRPGSpellRow> Spellbook;
	TArray<FGolemRPGRuneRow> Runes;
	TArray<FGolemRPGTalentLoadoutRow> TalentLoadouts;
	bool bCanContinue = false;
	bool bCanEditBuild = false;
	bool bCommandPending = false;
	FText EditLockReason;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemRPGPanelAction, EGolemRPGPanelAction);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemRPGPanelCommand, const FGolemRPGPanelCommand&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemRPGPanelPreview, const FGolemSkillBuild&);
