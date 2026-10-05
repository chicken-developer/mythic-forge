#pragma once

#include "CoreMinimal.h"
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
	Close
};

/** Presentation-only snapshot. The player composition supplies values after domain subscriptions fire. */
struct KNIGHTUI_API FKnightRPGPanelSnapshot
{
	FKnightEntityId KnightId;
	FText Title;
	FText Objective;
	FText Inventory;
	FText Equipment;
	FText Skills;
	FText Stats;
	FText Feedback;
	bool bCanContinue = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightRPGPanelAction, EKnightRPGPanelAction);
