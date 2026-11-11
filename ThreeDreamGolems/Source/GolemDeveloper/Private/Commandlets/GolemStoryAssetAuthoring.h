#pragma once

class UWorld;

namespace GolemStoryAssetAuthoring
{
	bool CreateStoryAssets();
	bool PopulateStoryMap(UWorld* World, int32 Milestone);
}
