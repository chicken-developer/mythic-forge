#include "Definitions/KnightCampaignDefinition.h"
#include "Types/KnightStableIds.h"

const FKnightQuestDefinition* UKnightCampaignDefinition::FindQuest(const FName QuestId) const
{
	return Quests.FindByPredicate([QuestId](const FKnightQuestDefinition& Quest)
	{
		return Quest.QuestId == QuestId;
	});
}

FKnightResult UKnightCampaignDefinition::ValidateCampaign() const
{
	if (!ValidateIdentity().IsSuccess() || !KnightIdentity::IsValidIdentifier(CampaignId)
		|| !KnightIdentity::IsValidIdentifier(ScenarioKind) || ContentRevision < 1 || Quests.IsEmpty())
	{
		return FKnightResult::Failure(EKnightError::InvalidData, CampaignId, FText::GetEmpty());
	}
	TSet<FName> Ids;
	TSet<FName> Objectives;
	TSet<FName> Acts;
	for (const FName Act : ActIds)
	{
		if (!KnightIdentity::IsValidIdentifier(Act) || Acts.Contains(Act))
		{
			return FKnightResult::Failure(EKnightError::DuplicateId, Act, FText::GetEmpty());
		}
		Acts.Add(Act);
	}
	for (const FKnightQuestDefinition& Quest : Quests)
	{
		if (!KnightIdentity::IsValidIdentifier(Quest.QuestId) || Ids.Contains(Quest.QuestId)
			|| !Acts.Contains(Quest.ActId) || Quest.Objectives.IsEmpty()
			|| Quest.IntroducedRevision < 1 || Quest.IntroducedRevision > ContentRevision)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, Quest.QuestId, FText::GetEmpty());
		}
		Ids.Add(Quest.QuestId);
		for (const FKnightObjectiveDefinition& Objective : Quest.Objectives)
		{
			if (!KnightIdentity::IsValidIdentifier(Objective.ObjectiveId) || Objectives.Contains(Objective.ObjectiveId)
				|| Objective.EventName.IsNone() || Objective.SubjectId.IsNone()
				|| Objective.RequiredCount < 1 || Objective.RequiredCount > 10000)
			{
				return FKnightResult::Failure(EKnightError::InvalidData, Objective.ObjectiveId, FText::GetEmpty());
			}
			Objectives.Add(Objective.ObjectiveId);
		}
	}
	TSet<FName> Visited;
	TSet<FName> Visiting;
	TFunction<bool(FName)> Visit = [&](const FName Id)
	{
		if (Visited.Contains(Id))
		{
			return true;
		}
		const FKnightQuestDefinition* Quest = FindQuest(Id);
		if (Quest == nullptr || Visiting.Contains(Id))
		{
			return false;
		}
		Visiting.Add(Id);
		TSet<FName> Parents;
		for (const FName Parent : Quest->Prerequisites)
		{
			if (Parents.Contains(Parent) || !Visit(Parent))
			{
				return false;
			}
			Parents.Add(Parent);
		}
		Visiting.Remove(Id);
		Visited.Add(Id);
		return true;
	};
	for (const FKnightQuestDefinition& Quest : Quests)
	{
		if (!Visit(Quest.QuestId))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, Quest.QuestId, FText::GetEmpty());
		}
	}
	return FKnightResult::Success();
}
