#include "Checkpoint/GolemCheckpointSave.h"

FGolemResult FGolemCampaignCheckpoint::ValidateScope() const
{
	if (EnvelopeVersion != 1 || !CheckpointId.IsValid() || !GolemIdentity::IsValidIdentifier(Quest.CampaignId)
		|| !GolemIdentity::IsValidIdentifier(Quest.ScenarioKind) || !Quest.RunId.IsValid()
		|| World.CampaignId != Quest.CampaignId || World.RunId != Quest.RunId
		|| World.FactRevision != Quest.FactRevision || Quest.Revision < 1 || World.Revision < 1
		|| Quest.ContentRevision < 1 || World.FactRevision < 0 || Dialogue.bActive || World.bSceneActive
		|| World.EncounterPhase == EGolemStoryEncounterPhase::Active
		|| (RPG.View.bGearDemo && RPG.View.DemoRunId != Quest.RunId)
		|| RPG.View.Golems.Num() != 3 || MapPackage.IsEmpty())
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("ConsistentCheckpoint"),
			NSLOCTEXT("GolemSave", "InvalidCheckpoint", "The campaign checkpoint is inconsistent or unsupported."));
	}
	for (const FGolemQuestProgress& Progress : Quest.Quests)
	{
		if (Progress.RewardReceipt.IsValid() && !RPG.View.ClaimedRewards.Contains(Progress.RewardReceipt))
		{
			return FGolemResult::Failure(EGolemError::Conflict, Progress.QuestId, FText::GetEmpty());
		}
	}
	TSet<FGolemEntityId> Golems;
	for (const FGolemRPGGolemView& Golem : RPG.View.Golems)
	{
		const float* Value = Poise.Find(Golem.GolemId);
		if (!Golem.GolemId.IsValid() || Golems.Contains(Golem.GolemId) || Value == nullptr
			|| !FMath::IsFinite(*Value) || *Value < 0.0f)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("CanonicalCheckpoint"), FText::GetEmpty());
		}
		Golems.Add(Golem.GolemId);
	}
	return FGolemResult::Success();
}
