#pragma once

#include "Models/KnightRPGPanelTypes.h"
#include "Models/KnightRPGModels.h"

class AKnightCharacter;
class APlayerController;
class UKnightRPGComponent;
struct FKnightGearDemoSnapshot;

namespace KnightRPGGearPresentation
{
	void Enrich(FKnightRPGPanelSnapshot& Snapshot, const FKnightRPGView& Party,
		const FKnightInventoryView& Inventory, const AKnightCharacter* Knight, const APlayerController* Caller,
		bool bAtSanctuary, const FKnightGearDemoSnapshot* Demo);
	FKnightGearCompare Compare(const UKnightRPGComponent* RPG, const FKnightInventoryView& Inventory,
		const AKnightCharacter* Knight, const FKnightGearPanelIntent& Intent,
		const AKnightCharacter* SourceKnight = nullptr, const AKnightCharacter* TargetKnight = nullptr);
	TArray<FKnightGearQuickUseRow> QuickUses(const FKnightRPGKnightView& Knight,
		const FKnightInventoryView& Inventory, const AKnightCharacter* Actor);
}
