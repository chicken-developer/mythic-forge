#include "Definitions/GolemCampaignDefinition.h"
#include "Types/GolemStableIds.h"

const FGolemQuestDefinition* UGolemCampaignDefinition::FindQuest(const FName QuestId) const
{
	return Quests.FindByPredicate([QuestId](const FGolemQuestDefinition& Quest)
	{
		return Quest.QuestId == QuestId;
	});
}

FGolemResult UGolemCampaignDefinition::ValidateCampaign() const
{
	if (!ValidateIdentity().IsSuccess() || !GolemIdentity::IsValidIdentifier(CampaignId)
		|| !GolemIdentity::IsValidIdentifier(ScenarioKind) || ContentRevision < 1 || Quests.IsEmpty())
	{
		return FGolemResult::Failure(EGolemError::InvalidData, CampaignId, FText::GetEmpty());
	}
	TSet<FName> Ids;
	TSet<FName> Objectives;
	TSet<FName> Acts;
	for (const FName Act : ActIds)
	{
		if (!GolemIdentity::IsValidIdentifier(Act) || Acts.Contains(Act))
		{
			return FGolemResult::Failure(EGolemError::DuplicateId, Act, FText::GetEmpty());
		}
		Acts.Add(Act);
	}
	for (const FGolemQuestDefinition& Quest : Quests)
	{
		if (!GolemIdentity::IsValidIdentifier(Quest.QuestId) || Ids.Contains(Quest.QuestId)
			|| !Acts.Contains(Quest.ActId) || Quest.Objectives.IsEmpty()
			|| Quest.IntroducedRevision < 1 || Quest.IntroducedRevision > ContentRevision)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, Quest.QuestId, FText::GetEmpty());
		}
		Ids.Add(Quest.QuestId);
		for (const FGolemObjectiveDefinition& Objective : Quest.Objectives)
		{
			if (!GolemIdentity::IsValidIdentifier(Objective.ObjectiveId) || Objectives.Contains(Objective.ObjectiveId)
				|| Objective.EventName.IsNone() || Objective.SubjectId.IsNone()
				|| Objective.RequiredCount < 1 || Objective.RequiredCount > 10000)
			{
				return FGolemResult::Failure(EGolemError::InvalidData, Objective.ObjectiveId, FText::GetEmpty());
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
		const FGolemQuestDefinition* Quest = FindQuest(Id);
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
	for (const FGolemQuestDefinition& Quest : Quests)
	{
		if (!Visit(Quest.QuestId))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, Quest.QuestId, FText::GetEmpty());
		}
	}
	return FGolemResult::Success();
}
