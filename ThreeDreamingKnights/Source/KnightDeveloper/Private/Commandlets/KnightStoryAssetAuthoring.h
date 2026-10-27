#pragma once

class UWorld;

namespace KnightStoryAssetAuthoring
{
	bool CreateStoryAssets();
	bool PopulateStoryMap(UWorld* World, int32 Milestone);
}
