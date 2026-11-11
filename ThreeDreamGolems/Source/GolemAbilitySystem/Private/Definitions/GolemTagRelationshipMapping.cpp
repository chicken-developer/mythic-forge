#include "Definitions/GolemTagRelationshipMapping.h"

UGolemTagRelationshipMapping::UGolemTagRelationshipMapping()
{
	DefinitionType = TEXT("GolemTagRelationships");
}

const TArray<FGolemTagRelationship>& UGolemTagRelationshipMapping::GetRelationships() const
{
	return Relationships;
}

FGolemResult UGolemTagRelationshipMapping::TrySetRelationships(const TArray<FGolemTagRelationship>& InRelationships)
{
	const FGolemResult Validation = ValidateRelationships(InRelationships);
	if (Validation.IsSuccess())
	{
		Relationships = InRelationships;
	}
	return Validation;
}

FGolemResult UGolemTagRelationshipMapping::ValidateRelationships(const TArray<FGolemTagRelationship>& InRelationships)
{
	TSet<FGameplayTag> Seen;
	for (const FGolemTagRelationship& Relationship : InRelationships)
	{
		if (!Relationship.AbilityTag.IsValid())
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("TagRelationships"),
				NSLOCTEXT("GolemGAS", "InvalidRelationshipTag", "Every relationship requires an ability tag."));
		}
		if (Seen.Contains(Relationship.AbilityTag))
		{
			return FGolemResult::Failure(EGolemError::DuplicateId, TEXT("TagRelationships"),
				NSLOCTEXT("GolemGAS", "DuplicateRelationship", "A mapping cannot contain duplicate ability relationships."));
		}
		if (Relationship.ActivationRequiredTags.HasAny(Relationship.ActivationBlockedTags))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("TagRelationships"),
				NSLOCTEXT("GolemGAS", "ContradictoryRelationship", "Required tags cannot also block the same activation."));
		}
		Seen.Add(Relationship.AbilityTag);
	}
	return FGolemResult::Success();
}
