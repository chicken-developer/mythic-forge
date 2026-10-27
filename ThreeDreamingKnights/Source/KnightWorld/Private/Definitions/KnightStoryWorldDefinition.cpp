#include "Definitions/KnightStoryWorldDefinition.h"
#include "Types/KnightStableIds.h"

const FKnightMapRegion* UKnightStoryWorldDefinition::FindRegion(const FName RegionId) const
{
	return MapRegions.FindByPredicate([RegionId](const FKnightMapRegion& Region)
	{
		return Region.RegionId == RegionId;
	});
}

const FKnightMapMarker* UKnightStoryWorldDefinition::FindMarker(const FName MarkerId) const
{
	return MapMarkers.FindByPredicate([MarkerId](const FKnightMapMarker& Marker)
	{
		return Marker.MarkerId == MarkerId;
	});
}

const FKnightStoryInteraction* UKnightStoryWorldDefinition::FindInteraction(const FName AnchorId) const
{
	return Interactions.FindByPredicate([AnchorId](const FKnightStoryInteraction& Entry)
	{
		return Entry.AnchorId == AnchorId;
	});
}

FKnightResult UKnightStoryWorldDefinition::ValidateWorld() const
{
	TSet<FName> Ids;
	for (const FKnightStoryInteraction& Interaction : Interactions)
	{
		if (!KnightIdentity::IsValidIdentifier(Interaction.AnchorId) || Ids.Contains(Interaction.AnchorId)
			|| Interaction.Label.IsEmpty() || !FMath::IsFinite(Interaction.InteractionRange)
			|| Interaction.InteractionRange <= 0.0f || Interaction.InteractionRange > 1000.0f
			|| Interaction.Action > EKnightStoryAction::Branch
			|| !Interaction.RequiredObjectiveId.IsNone() && Interaction.RequiredQuestId.IsNone()
			|| Interaction.Action == EKnightStoryAction::Travel && Interaction.Destination.IsNull()
			|| Interaction.Action == EKnightStoryAction::Conversation
				&& !KnightIdentity::IsValidDefinitionId(Interaction.ConversationId)
			|| Interaction.Action == EKnightStoryAction::Deliver
				&& (!KnightIdentity::IsValidDefinitionId(Interaction.DeliveryItemId)
					|| Interaction.DeliveryCount < 1 || Interaction.DeliveryCount > 10000)
			|| !Interaction.AllowedChoices.IsEmpty() && !Interaction.AllowedChoices.Contains(Interaction.ValueId))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, Interaction.AnchorId, FText::GetEmpty());
		}
		Ids.Add(Interaction.AnchorId);
		if (Interaction.RequiredBranchId.IsNone() != Interaction.RequiredBranchChoice.IsNone()
			|| Interaction.Action == EKnightStoryAction::Branch
				&& (!KnightIdentity::IsValidIdentifier(Interaction.SubjectId) || Interaction.AllowedChoices.IsEmpty()))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, Interaction.AnchorId, FText::GetEmpty());
		}
		TSet<FName> Choices;
		for (const FName Choice : Interaction.AllowedChoices)
		{
			if (!KnightIdentity::IsValidIdentifier(Choice) || Choices.Contains(Choice))
			{
				return FKnightResult::Failure(EKnightError::DuplicateId, Choice, FText::GetEmpty());
			}
			Choices.Add(Choice);
		}
	}
	TSet<FName> Regions;
	for (const FKnightMapRegion& Region : MapRegions)
	{
		if (!Region.IsValid() || Regions.Contains(Region.RegionId))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, Region.RegionId, FText::GetEmpty());
		}
		Regions.Add(Region.RegionId);
	}
	TSet<FName> Markers;
	for (const FKnightMapMarker& Marker : MapMarkers)
	{
		const FKnightMapRegion* Region = FindRegion(Marker.RegionId);
		FVector2D UV;
		if (!KnightIdentity::IsValidIdentifier(Marker.MarkerId) || Markers.Contains(Marker.MarkerId)
			|| Marker.Label.IsEmpty() || Region == nullptr || !Region->Project(Marker.WorldPosition, UV)
			|| Marker.Kind > EKnightMapMarkerKind::Hazard || !FMath::IsFinite(Marker.DiscoveryRadius)
			|| Marker.DiscoveryRadius <= 0.0f || Marker.DiscoveryRadius > 3000.0f
			|| !Marker.AnchorId.IsNone() && !Ids.Contains(Marker.AnchorId)
			|| !Marker.TravelRegionId.IsNone() && FindRegion(Marker.TravelRegionId) == nullptr)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, Marker.MarkerId, FText::GetEmpty());
		}
		Markers.Add(Marker.MarkerId);
	}
	return ValidateIdentity();
}
