#pragma once

class UWorld;

namespace KnightCampaignAssetAuthoring
{
	bool CreateAssets();
	bool PopulateMap(UWorld* World, bool bCourtyard);
}
