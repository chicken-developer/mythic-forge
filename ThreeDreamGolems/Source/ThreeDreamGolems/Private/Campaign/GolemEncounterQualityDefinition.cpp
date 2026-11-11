#include "Campaign/GolemEncounterQualityDefinition.h"

UGolemEncounterQualityDefinition::UGolemEncounterQualityDefinition()
{
	DefinitionType = TEXT("GolemEncounterQuality");
}

FGolemResult UGolemEncounterQualityDefinition::ValidateEncounter() const
{
	if (Wave.IsEmpty() || Wave.Num() > 12 || !BossArchetypeId.IsValid() || !BossPhaseId.IsValid()
		|| StoryAttackBudget < 1 || StandardAttackBudget < StoryAttackBudget || StandardAttackBudget > 6)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("EncounterQuality"), FText::GetEmpty());
	}
	for (const FGolemEncounterPressure& Entry : Wave)
	{
		if (!Entry.ArchetypeId.IsValid() || Entry.Position.ContainsNaN())
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("EncounterPressure"), FText::GetEmpty());
		}
	}
	return FGolemResult::Success();
}
