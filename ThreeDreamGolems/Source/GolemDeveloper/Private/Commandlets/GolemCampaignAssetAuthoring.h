#pragma once

class UWorld;

namespace GolemCampaignAssetAuthoring
{
	bool CreateAssets();
	bool PopulateMap(UWorld* World, bool bCourtyard);
}
