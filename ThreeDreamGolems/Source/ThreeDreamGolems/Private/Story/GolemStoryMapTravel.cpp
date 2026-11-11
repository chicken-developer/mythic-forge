#include "Story/GolemStoryGameMode.h"

#include "Definitions/GolemStoryWorldDefinition.h"
#include "Story/GolemStoryState.h"

FGolemResult AGolemStoryGameMode::TryMapTravel(APlayerController* Player, const FName MarkerId,
	const FGolemEntityId& ExpectedGolem, const int64 ExpectedRevision)
{
	FGolemRPGCallerContext Caller;
	FGolemResult Result = BuildCaller(Player, Caller);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!Player->IsLocalController() || Caller.GolemId != ExpectedGolem || bTravelling || bStoryMutation
		|| Story == nullptr)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("HostTravel"),
			NSLOCTEXT("GolemMap", "HostTravel", "Only the host can move the party. Refresh the map and retry."));
	}
	if (Story->WorldState->GetSnapshot().Revision != ExpectedRevision)
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("MapRevision"),
			NSLOCTEXT("GolemMap", "MapRevision", "Map discovery changed. Review the map and retry."));
	}
	const FGolemMapMarker* Marker = WorldDefinition->FindMarker(MarkerId);
	const FGolemMapRegion* Region = Marker != nullptr ? WorldDefinition->FindRegion(Marker->TravelRegionId) : nullptr;
	if (Region == nullptr || !Story->WorldState->IsMarkerDiscovered(MarkerId)
		|| Marker->RegionId != Story->WorldState->GetActiveRegionId()
		|| Region->RegionId == Story->WorldState->GetActiveRegionId()
		|| !Story->WorldState->IsRegionDiscovered(Region->RegionId))
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("UndiscoveredTravel"),
			NSLOCTEXT("GolemMap", "UndiscoveredTravel", "Visit and discover the destination before using fast travel."));
	}
	return TravelStory(Player, Region->Map.ToSoftObjectPath().GetLongPackageName());
}
