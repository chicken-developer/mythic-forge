#pragma once

#include "CoreMinimal.h"
#include "Models/KnightGearPanelTypes.h"
#include "Skills/KnightSkillTypes.h"
#include "Types/KnightStableIds.h"

enum class EKnightRPGPanelAction : uint8
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
	SelectKnight
};

enum class EKnightRPGPanelTab : uint8
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
struct KNIGHTUI_API FKnightRPGPanelCommand
{
	EKnightRPGPanelAction Action = EKnightRPGPanelAction::ApplyBuild;
	FKnightEntityId KnightId;
	int64 ExpectedRevision = 0;
	FGuid RequestId;
	FKnightSkillBuild ProposedBuild;
	FGuid WeaponRuneInstanceId;
	FGuid SkillRuneInstanceId;
	FGuid TalentLoadoutId;
	FString TalentLoadoutName;
};

struct KNIGHTUI_API FKnightRPGStatRow
{
	FText Group;
	FText Name;
	FText Value;
	FText Contributions;
};

struct KNIGHTUI_API FKnightRPGSpellRow
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

struct KNIGHTUI_API FKnightRPGSpecRow
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

struct KNIGHTUI_API FKnightRPGRuneRow
{
	FName RuneId;
	FGuid ItemInstanceId;
	FText Name;
	FText Description;
	FText IncompatibilityReason;
	bool bWeaponSocket = false;
	bool bCompatible = false;
};

struct KNIGHTUI_API FKnightRPGTalentLoadoutRow
{
	FGuid LoadoutId;
	FString Name;
	FKnightSkillBuild Build;
	int32 Version = 1;
};

struct KNIGHTUI_API FKnightRPGKnightChoice
{
	FKnightEntityId KnightId;
	FText Name;
};

/** Pure definition calculation, supplied without applying effects to a live actor. */
struct KNIGHTUI_API FKnightRPGBuildPreview
{
	FKnightEntityId KnightId;
	FKnightSkillBuild Build;
	TArray<FKnightRPGSpellRow> Spells;
	TArray<FKnightRPGRuneRow> Runes;
	FText Summary;
};

/** Confirmed state only; populated after domain subscriptions or explicit command responses. */
struct KNIGHTUI_API FKnightRPGPanelSnapshot
{
	FKnightGearPanelSnapshot Gear;
	FKnightEntityId KnightId;
	FText Title;
	FText Objective;
	FText Inventory;
	FText Equipment;
	FText WeaponComparison;
	FText RuneComparison;
	FText Skills;
	FText Stats;
	FText Feedback;
	FKnightSkillBuild Build;
	int64 Revision = 0;
	int32 AvailablePoints = 0;
	FGuid WeaponRuneInstanceId;
	FGuid SkillRuneInstanceId;
	FGuid ActiveTalentLoadoutId;
	FGuid LastCompletedRequestId;
	TArray<FKnightRPGKnightChoice> Knights;
	TArray<FKnightRPGStatRow> StatRows;
	TArray<FKnightRPGSpecRow> Specializations;
	TArray<FKnightRPGSpellRow> Spellbook;
	TArray<FKnightRPGRuneRow> Runes;
	TArray<FKnightRPGTalentLoadoutRow> TalentLoadouts;
	bool bCanContinue = false;
	bool bCanEditBuild = false;
	bool bCommandPending = false;
	FText EditLockReason;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightRPGPanelAction, EKnightRPGPanelAction);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightRPGPanelCommand, const FKnightRPGPanelCommand&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightRPGPanelPreview, const FKnightSkillBuild&);
