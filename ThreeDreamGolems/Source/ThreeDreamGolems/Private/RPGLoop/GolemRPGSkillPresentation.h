#pragma once

#include "Models/GolemRPGPanelTypes.h"
#include "Models/GolemRPGModels.h"

class AGolemCharacter;

namespace GolemRPGSkillPresentation
{
	void Enrich(FGolemRPGPanelSnapshot& Snapshot, const FGolemRPGGolemView& GolemView,
		const FGolemRPGView& PartyView, const FGolemInventoryView& Inventory, const AGolemCharacter* Golem);
	FGolemRPGBuildPreview Preview(const FGolemSkillBuild& Build, float Power);
}
