#include "Checkpoint/KnightCheckpointSave.h"

FKnightResult FKnightCampaignCheckpoint::ValidateScope() const
{
	if (EnvelopeVersion != 1 || !CheckpointId.IsValid() || !KnightIdentity::IsValidIdentifier(Quest.CampaignId)
		|| !KnightIdentity::IsValidIdentifier(Quest.ScenarioKind) || !Quest.RunId.IsValid()
		|| World.CampaignId != Quest.CampaignId || World.RunId != Quest.RunId
		|| World.FactRevision != Quest.FactRevision || Quest.Revision < 1 || World.Revision < 1
		|| Quest.ContentRevision < 1 || World.FactRevision < 0 || Dialogue.bActive || World.bSceneActive
		|| World.EncounterPhase == EKnightStoryEncounterPhase::Active
		|| (RPG.View.bGearDemo && RPG.View.DemoRunId != Quest.RunId)
		|| RPG.View.Knights.Num() != 3 || MapPackage.IsEmpty())
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("ConsistentCheckpoint"),
			NSLOCTEXT("KnightSave", "InvalidCheckpoint", "The campaign checkpoint is inconsistent or unsupported."));
	}
	for (const FKnightQuestProgress& Progress : Quest.Quests)
	{
		if (Progress.RewardReceipt.IsValid() && !RPG.View.ClaimedRewards.Contains(Progress.RewardReceipt))
		{
			return FKnightResult::Failure(EKnightError::Conflict, Progress.QuestId, FText::GetEmpty());
		}
	}
	TSet<FKnightEntityId> Knights;
	for (const FKnightRPGKnightView& Knight : RPG.View.Knights)
	{
		const float* Value = Poise.Find(Knight.KnightId);
		if (!Knight.KnightId.IsValid() || Knights.Contains(Knight.KnightId) || Value == nullptr
			|| !FMath::IsFinite(*Value) || *Value < 0.0f)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("CanonicalCheckpoint"), FText::GetEmpty());
		}
		Knights.Add(Knight.KnightId);
	}
	return FKnightResult::Success();
}
