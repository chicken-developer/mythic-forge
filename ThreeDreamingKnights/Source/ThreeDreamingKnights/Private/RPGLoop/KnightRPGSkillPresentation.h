#pragma once

#include "Models/KnightRPGPanelTypes.h"
#include "Models/KnightRPGModels.h"

class AKnightCharacter;

namespace KnightRPGSkillPresentation
{
	void Enrich(FKnightRPGPanelSnapshot& Snapshot, const FKnightRPGKnightView& KnightView,
		const FKnightRPGView& PartyView, const FKnightInventoryView& Inventory, const AKnightCharacter* Knight);
	FKnightRPGBuildPreview Preview(const FKnightSkillBuild& Build, float Power);
}
