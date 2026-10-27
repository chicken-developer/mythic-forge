#include "Story/KnightStoryGameMode.h"

#include "Definitions/KnightStoryWorldDefinition.h"
#include "Story/KnightStoryState.h"

FKnightResult AKnightStoryGameMode::TryMapTravel(APlayerController* Player, const FName MarkerId,
	const FKnightEntityId& ExpectedKnight, const int64 ExpectedRevision)
{
	FKnightRPGCallerContext Caller;
	FKnightResult Result = BuildCaller(Player, Caller);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!Player->IsLocalController() || Caller.KnightId != ExpectedKnight || bTravelling || bStoryMutation
		|| Story == nullptr)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("HostTravel"),
			NSLOCTEXT("KnightMap", "HostTravel", "Only the host can move the party. Refresh the map and retry."));
	}
	if (Story->WorldState->GetSnapshot().Revision != ExpectedRevision)
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("MapRevision"),
			NSLOCTEXT("KnightMap", "MapRevision", "Map discovery changed. Review the map and retry."));
	}
	const FKnightMapMarker* Marker = WorldDefinition->FindMarker(MarkerId);
	const FKnightMapRegion* Region = Marker != nullptr ? WorldDefinition->FindRegion(Marker->TravelRegionId) : nullptr;
	if (Region == nullptr || !Story->WorldState->IsMarkerDiscovered(MarkerId)
		|| Marker->RegionId != Story->WorldState->GetActiveRegionId()
		|| Region->RegionId == Story->WorldState->GetActiveRegionId()
		|| !Story->WorldState->IsRegionDiscovered(Region->RegionId))
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("UndiscoveredTravel"),
			NSLOCTEXT("KnightMap", "UndiscoveredTravel", "Visit and discover the destination before using fast travel."));
	}
	return TravelStory(Player, Region->Map.ToSoftObjectPath().GetLongPackageName());
}
