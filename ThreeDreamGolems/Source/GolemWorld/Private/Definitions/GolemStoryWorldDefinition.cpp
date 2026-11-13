#include "Definitions/GolemStoryWorldDefinition.h"
#include "Types/GolemStableIds.h"

const FGolemMapRegion* UGolemStoryWorldDefinition::FindRegion(const FName RegionId) const
{
	return MapRegions.FindByPredicate([RegionId](const FGolemMapRegion& Region)
	{
		return Region.RegionId == RegionId;
	});
}

const FGolemMapMarker* UGolemStoryWorldDefinition::FindMarker(const FName MarkerId) const
{
	return MapMarkers.FindByPredicate([MarkerId](const FGolemMapMarker& Marker)
	{
		return Marker.MarkerId == MarkerId;
	});
}

const FGolemStoryInteraction* UGolemStoryWorldDefinition::FindInteraction(const FName AnchorId) const
{
	return Interactions.FindByPredicate([AnchorId](const FGolemStoryInteraction& Entry)
	{
		return Entry.AnchorId == AnchorId;
	});
}

FGolemResult UGolemStoryWorldDefinition::ValidateWorld() const
{
	TSet<FName> Ids;
	for (const FGolemStoryInteraction& Interaction : Interactions)
	{
		if (!GolemIdentity::IsValidIdentifier(Interaction.AnchorId) || Ids.Contains(Interaction.AnchorId)
			|| Interaction.Label.IsEmpty() || !FMath::IsFinite(Interaction.InteractionRange)
			|| Interaction.InteractionRange <= 0.0f || Interaction.InteractionRange > 1000.0f
			|| Interaction.Action > EGolemStoryAction::Branch
			|| !Interaction.RequiredObjectiveId.IsNone() && Interaction.RequiredQuestId.IsNone()
			|| Interaction.Action == EGolemStoryAction::Travel && Interaction.Destination.IsNull()
			|| Interaction.Action == EGolemStoryAction::Conversation
				&& !GolemIdentity::IsValidDefinitionId(Interaction.ConversationId)
			|| Interaction.Action == EGolemStoryAction::Deliver
				&& (!GolemIdentity::IsValidDefinitionId(Interaction.DeliveryItemId)
					|| Interaction.DeliveryCount < 1 || Interaction.DeliveryCount > 10000)
			|| !Interaction.AllowedChoices.IsEmpty() && !Interaction.AllowedChoices.Contains(Interaction.ValueId))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, Interaction.AnchorId, FText::GetEmpty());
		}
		Ids.Add(Interaction.AnchorId);
		if (Interaction.RequiredBranchId.IsNone() != Interaction.RequiredBranchChoice.IsNone()
			|| Interaction.Action == EGolemStoryAction::Branch
				&& (!GolemIdentity::IsValidIdentifier(Interaction.SubjectId) || Interaction.AllowedChoices.IsEmpty()))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, Interaction.AnchorId, FText::GetEmpty());
		}
		TSet<FName> Choices;
		for (const FName Choice : Interaction.AllowedChoices)
		{
			if (!GolemIdentity::IsValidIdentifier(Choice) || Choices.Contains(Choice))
			{
				return FGolemResult::Failure(EGolemError::DuplicateId, Choice, FText::GetEmpty());
			}
			Choices.Add(Choice);
		}
	}
	TSet<FName> Regions;
	for (const FGolemMapRegion& Region : MapRegions)
	{
		if (!Region.IsValid() || Regions.Contains(Region.RegionId))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, Region.RegionId, FText::GetEmpty());
		}
		Regions.Add(Region.RegionId);
	}
	TSet<FName> Markers;
	for (const FGolemMapMarker& Marker : MapMarkers)
	{
		const FGolemMapRegion* Region = FindRegion(Marker.RegionId);
		FVector2D UV;
		if (!GolemIdentity::IsValidIdentifier(Marker.MarkerId) || Markers.Contains(Marker.MarkerId)
			|| Marker.Label.IsEmpty() || Region == nullptr || !Region->Project(Marker.WorldPosition, UV)
			|| Marker.Kind > EGolemMapMarkerKind::Hazard || !FMath::IsFinite(Marker.DiscoveryRadius)
			|| Marker.DiscoveryRadius <= 0.0f || Marker.DiscoveryRadius > 3000.0f
			|| !Marker.AnchorId.IsNone() && !Ids.Contains(Marker.AnchorId)
			|| !Marker.TravelRegionId.IsNone() && FindRegion(Marker.TravelRegionId) == nullptr)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, Marker.MarkerId, FText::GetEmpty());
		}
		Markers.Add(Marker.MarkerId);
	}
	return ValidateIdentity();
}
