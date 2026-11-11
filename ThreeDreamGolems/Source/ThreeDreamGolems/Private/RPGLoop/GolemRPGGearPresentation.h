#pragma once

#include "Models/GolemRPGPanelTypes.h"
#include "Models/GolemRPGModels.h"

class AGolemCharacter;
class APlayerController;
class UGolemRPGComponent;
struct FGolemGearDemoSnapshot;

namespace GolemRPGGearPresentation
{
	void Enrich(FGolemRPGPanelSnapshot& Snapshot, const FGolemRPGView& Party,
		const FGolemInventoryView& Inventory, const AGolemCharacter* Golem, const APlayerController* Caller,
		bool bAtSanctuary, const FGolemGearDemoSnapshot* Demo);
	FGolemGearCompare Compare(const UGolemRPGComponent* RPG, const FGolemInventoryView& Inventory,
		const AGolemCharacter* Golem, const FGolemGearPanelIntent& Intent,
		const AGolemCharacter* SourceGolem = nullptr, const AGolemCharacter* TargetGolem = nullptr);
	TArray<FGolemGearQuickUseRow> QuickUses(const FGolemRPGGolemView& Golem,
		const FGolemInventoryView& Inventory, const AGolemCharacter* Actor);
}
