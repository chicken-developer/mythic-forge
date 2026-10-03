#include "Definitions/KnightTagRelationshipMapping.h"

UKnightTagRelationshipMapping::UKnightTagRelationshipMapping()
{
	DefinitionType = TEXT("KnightTagRelationships");
}

const TArray<FKnightTagRelationship>& UKnightTagRelationshipMapping::GetRelationships() const
{
	return Relationships;
}

FKnightResult UKnightTagRelationshipMapping::TrySetRelationships(const TArray<FKnightTagRelationship>& InRelationships)
{
	const FKnightResult Validation = ValidateRelationships(InRelationships);
	if (Validation.IsSuccess())
	{
		Relationships = InRelationships;
	}
	return Validation;
}

FKnightResult UKnightTagRelationshipMapping::ValidateRelationships(const TArray<FKnightTagRelationship>& InRelationships)
{
	TSet<FGameplayTag> Seen;
	for (const FKnightTagRelationship& Relationship : InRelationships)
	{
		if (!Relationship.AbilityTag.IsValid())
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("TagRelationships"),
				NSLOCTEXT("KnightGAS", "InvalidRelationshipTag", "Every relationship requires an ability tag."));
		}
		if (Seen.Contains(Relationship.AbilityTag))
		{
			return FKnightResult::Failure(EKnightError::DuplicateId, TEXT("TagRelationships"),
				NSLOCTEXT("KnightGAS", "DuplicateRelationship", "A mapping cannot contain duplicate ability relationships."));
		}
		if (Relationship.ActivationRequiredTags.HasAny(Relationship.ActivationBlockedTags))
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("TagRelationships"),
				NSLOCTEXT("KnightGAS", "ContradictoryRelationship", "Required tags cannot also block the same activation."));
		}
		Seen.Add(Relationship.AbilityTag);
	}
	return FKnightResult::Success();
}
