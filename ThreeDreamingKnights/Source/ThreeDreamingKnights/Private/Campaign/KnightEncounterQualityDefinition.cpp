#include "Campaign/KnightEncounterQualityDefinition.h"

UKnightEncounterQualityDefinition::UKnightEncounterQualityDefinition()
{
	DefinitionType = TEXT("KnightEncounterQuality");
}

FKnightResult UKnightEncounterQualityDefinition::ValidateEncounter() const
{
	if (Wave.IsEmpty() || Wave.Num() > 12 || !BossArchetypeId.IsValid() || !BossPhaseId.IsValid()
		|| StoryAttackBudget < 1 || StandardAttackBudget < StoryAttackBudget || StandardAttackBudget > 6)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("EncounterQuality"), FText::GetEmpty());
	}
	for (const FKnightEncounterPressure& Entry : Wave)
	{
		if (!Entry.ArchetypeId.IsValid() || Entry.Position.ContainsNaN())
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("EncounterPressure"), FText::GetEmpty());
		}
	}
	return FKnightResult::Success();
}
