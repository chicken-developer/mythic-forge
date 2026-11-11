#include "Runtime/GolemQuestComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UGolemQuestComponent::UGolemQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGolemQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGolemQuestComponent, Snapshot);
}

FGolemResult UGolemQuestComponent::InitializeCampaign(UGolemCampaignDefinition* Definition, const FGuid& RunId)
{
	if (!GetOwner()->HasAuthority() || Campaign != nullptr || Definition == nullptr
		|| !RunId.IsValid() || !Definition->ValidateCampaign().IsSuccess())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("CampaignInitialize"), FText::GetEmpty());
	}
	Campaign = Definition;
	Snapshot.DefinitionId = Campaign->GetPrimaryAssetId();
	Snapshot.CampaignId = Campaign->CampaignId;
	Snapshot.ScenarioKind = Campaign->ScenarioKind;
	Snapshot.ContentRevision = Campaign->ContentRevision;
	Snapshot.RunId = RunId;
	for (const FGolemQuestDefinition& DefinitionRow : Campaign->Quests)
	{
		FGolemQuestProgress& Row = Snapshot.Quests.AddDefaulted_GetRef();
		Row.QuestId = DefinitionRow.QuestId;
		for (const FGolemObjectiveDefinition& Objective : DefinitionRow.Objectives)
		{
			Row.Counts.Add(Objective.ObjectiveId, 0);
		}
	}
	RefreshAvailability(true);
	Subscription = GetWorld()->GetSubsystem<UGolemEventSubsystem>()->Subscribe<FGolemStoryEvent>(this,
		[this](const FGolemStoryEvent& Fact)
		{
			HandleFact(Fact);
		});
	PublishSnapshot();
	return FGolemResult::Success();
}

const FGolemQuestSnapshot& UGolemQuestComponent::GetSnapshot() const
{
	return Snapshot;
}

const FGolemQuestProgress* UGolemQuestComponent::FindProgress(const FName QuestId) const
{
	return Snapshot.Quests.FindByPredicate([QuestId](const FGolemQuestProgress& Row)
	{
		return Row.QuestId == QuestId;
	});
}

void UGolemQuestComponent::RefreshAvailability(const bool bActivateAutomatically)
{
	for (FGolemQuestProgress& Row : Snapshot.Quests)
	{
		if (Row.Phase != EGolemQuestPhase::Locked)
		{
			continue;
		}
		const FGolemQuestDefinition* Definition = Campaign->FindQuest(Row.QuestId);
		bool bReady = true;
		for (const FName Parent : Definition->Prerequisites)
		{
			const FGolemQuestProgress* Progress = FindProgress(Parent);
			bReady &= Progress != nullptr && Progress->Phase == EGolemQuestPhase::Complete;
		}
		if (bReady)
		{
			Row.Phase = bActivateAutomatically && Definition->bAutoActivate
				? EGolemQuestPhase::Active : EGolemQuestPhase::Available;
			Row.ActivatedRevision = Snapshot.FactRevision;
		}
	}
}

FGolemResult UGolemQuestComponent::ActivateQuest(const FName QuestId)
{
	FGolemQuestProgress* Row = Snapshot.Quests.FindByPredicate([QuestId](const FGolemQuestProgress& Entry)
	{
		return Entry.QuestId == QuestId;
	});
	if (!GetOwner()->HasAuthority() || Row == nullptr || Row->Phase != EGolemQuestPhase::Available)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, QuestId, FText::GetEmpty());
	}
	Row->Phase = EGolemQuestPhase::Active;
	Row->ActivatedRevision = Snapshot.FactRevision;
	PublishSnapshot();
	return FGolemResult::Success();
}

void UGolemQuestComponent::HandleFact(const FGolemStoryEvent& Fact)
{
	if (!GetOwner()->HasAuthority() || Fact.CampaignId != Snapshot.CampaignId || Fact.RunId != Snapshot.RunId
		|| !Fact.ConfirmationId.IsValid() || Snapshot.SeenFacts.Contains(Fact.ConfirmationId)
		|| Fact.Revision <= Snapshot.FactRevision || Fact.Quantity <= 0)
	{
		return;
	}
	Snapshot.SeenFacts.Add(Fact.ConfirmationId);
	Snapshot.FactRevision = Fact.Revision;
	ApplyFact(Fact, false);
	PublishSnapshot();
}

bool UGolemQuestComponent::ApplyFact(const FGolemStoryEvent& Fact, const bool bCurrentState)
{
	bool bChanged = false;
	for (FGolemQuestProgress& Row : Snapshot.Quests)
	{
		if (Row.Phase != EGolemQuestPhase::Active)
		{
			continue;
		}
		const FGolemQuestDefinition* Definition = Campaign->FindQuest(Row.QuestId);
		for (const FGolemObjectiveDefinition& Objective : Definition->Objectives)
		{
			int32& Count = Row.Counts.FindChecked(Objective.ObjectiveId);
			if (Count >= Objective.RequiredCount)
			{
				continue;
			}
			const bool bMatches = Objective.EventName == Fact.EventName && Objective.SubjectId == Fact.SubjectId
				&& (Objective.ValueId.IsNone() || Objective.ValueId == Fact.ValueId)
				&& (bCurrentState ? Objective.Policy == EGolemObjectivePolicy::CurrentState
					: Fact.Revision > Row.ActivatedRevision)
				&& (!Objective.bEncounterScoped || EncounterRun.IsValid() && Fact.EncounterRunId == EncounterRun);
			if (bMatches)
			{
				Count = bCurrentState ? FMath::Min(Fact.Quantity, Objective.RequiredCount)
					: Count + FMath::Min(Fact.Quantity, Objective.RequiredCount - Count);
				bChanged = true;
			}
			if (Definition->bOrdered)
			{
				break;
			}
		}
		bool bComplete = true;
		for (const FGolemObjectiveDefinition& Objective : Definition->Objectives)
		{
			bComplete &= Row.Counts.FindChecked(Objective.ObjectiveId) == Objective.RequiredCount;
		}
		if (bComplete)
		{
			Row.Phase = Definition->RewardKey.IsNone() ? EGolemQuestPhase::Complete : EGolemQuestPhase::RewardPending;
		}
	}
	RefreshAvailability(true);
	return bChanged;
}

FGolemResult UGolemQuestComponent::ApplyCurrentFact(const FGolemStoryEvent& Fact)
{
	if (!GetOwner()->HasAuthority() || Fact.CampaignId != Snapshot.CampaignId || Fact.RunId != Snapshot.RunId)
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, Fact.SubjectId, FText::GetEmpty());
	}
	if (ApplyFact(Fact, true))
	{
		PublishSnapshot();
	}
	return FGolemResult::Success();
}

FGolemResult UGolemQuestComponent::CommitReward(const FName QuestId, const FGuid& Receipt)
{
	FGolemQuestProgress* Row = Snapshot.Quests.FindByPredicate([QuestId](const FGolemQuestProgress& Entry)
	{
		return Entry.QuestId == QuestId;
	});
	if (!GetOwner()->HasAuthority() || Row == nullptr || !Receipt.IsValid()
		|| (Row->Phase != EGolemQuestPhase::RewardPending && Row->RewardReceipt != Receipt))
	{
		return FGolemResult::Failure(EGolemError::InvalidState, QuestId, FText::GetEmpty());
	}
	if (Row->Phase == EGolemQuestPhase::Complete)
	{
		return FGolemResult::Success();
	}
	Row->RewardReceipt = Receipt;
	Row->Phase = EGolemQuestPhase::Complete;
	RefreshAvailability(true);
	PublishSnapshot();
	return FGolemResult::Success();
}

FGolemResult UGolemQuestComponent::CommitBranch(const FName BranchId, const FName ChoiceId,
	const int64 ExpectedRevision)
{
	if (!GetOwner()->HasAuthority() || !GolemIdentity::IsValidIdentifier(BranchId)
		|| !GolemIdentity::IsValidIdentifier(ChoiceId) || ExpectedRevision != Snapshot.Revision)
	{
		return FGolemResult::Failure(EGolemError::InvalidRevision, BranchId, FText::GetEmpty());
	}
	if (const FName* Previous = Snapshot.Branches.Find(BranchId))
	{
		return *Previous == ChoiceId ? FGolemResult::Success()
			: FGolemResult::Failure(EGolemError::Conflict, BranchId, FText::GetEmpty());
	}
	Snapshot.Branches.Add(BranchId, ChoiceId);
	PublishSnapshot();
	return FGolemResult::Success();
}

FGolemResult UGolemQuestComponent::SetEncounterRun(const FGuid& EncounterRunId)
{
	if (!GetOwner()->HasAuthority() || !EncounterRunId.IsValid())
	{
		return FGolemResult::Failure(EGolemError::InvalidId, TEXT("EncounterGeneration"), FText::GetEmpty());
	}
	EncounterRun = EncounterRunId;
	return FGolemResult::Success();
}

FGolemResult UGolemQuestComponent::ValidateSnapshot(const FGolemQuestSnapshot& Candidate) const
{
	if (Campaign == nullptr || Candidate.CampaignId != Campaign->CampaignId
		|| Candidate.ScenarioKind != Campaign->ScenarioKind || !Candidate.RunId.IsValid()
		|| Candidate.Revision < 1 || Candidate.FactRevision < 0
		|| Candidate.ContentRevision < 1 || Candidate.ContentRevision > Campaign->ContentRevision)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("QuestSnapshotScope"), FText::GetEmpty());
	}
	TSet<FName> Ids;
	TSet<FGuid> Facts;
	for (const FGuid Id : Candidate.SeenFacts)
	{
		if (!Id.IsValid() || Facts.Contains(Id))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("QuestFacts"), FText::GetEmpty());
		}
		Facts.Add(Id);
	}
	for (const FGolemQuestProgress& Row : Candidate.Quests)
	{
		const FGolemQuestDefinition* Definition = Campaign->FindQuest(Row.QuestId);
		if (Definition == nullptr || Ids.Contains(Row.QuestId) || Row.Counts.Num() != Definition->Objectives.Num()
			|| Row.ActivatedRevision < 0 || Row.ActivatedRevision > Candidate.FactRevision
			|| Row.Phase > EGolemQuestPhase::Complete)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, Row.QuestId, FText::GetEmpty());
		}
		Ids.Add(Row.QuestId);
		bool bFinished = true;
		bool bFoundIncomplete = false;
		for (const FGolemObjectiveDefinition& Objective : Definition->Objectives)
		{
			const int32* Count = Row.Counts.Find(Objective.ObjectiveId);
			if (Count == nullptr || *Count < 0 || *Count > Objective.RequiredCount
				|| Definition->bOrdered && bFoundIncomplete && *Count > 0)
			{
				return FGolemResult::Failure(EGolemError::InvalidData, Objective.ObjectiveId, FText::GetEmpty());
			}
			bFinished &= *Count == Objective.RequiredCount;
			bFoundIncomplete |= *Count < Objective.RequiredCount;
		}
		if ((Row.Phase >= EGolemQuestPhase::RewardPending && !bFinished)
			|| (Row.Phase == EGolemQuestPhase::Complete && !Definition->RewardKey.IsNone()
				&& !Row.RewardReceipt.IsValid()))
		{
			return FGolemResult::Failure(EGolemError::InvalidData, Row.QuestId, FText::GetEmpty());
		}
	}
	for (const FGolemQuestDefinition& Definition : Campaign->Quests)
	{
		if (Definition.IntroducedRevision <= Candidate.ContentRevision && !Ids.Contains(Definition.QuestId))
		{
			return FGolemResult::Failure(EGolemError::NotFound, Definition.QuestId, FText::GetEmpty());
		}
	}
	// Removed content is an error. Additions are deliberately allowed and become Available on load.
	return FGolemResult::Success();
}

FGolemResult UGolemQuestComponent::RestoreSnapshot(const FGolemQuestSnapshot& Candidate)
{
	FGolemResult Result = ValidateSnapshot(Candidate);
	if (!GetOwner()->HasAuthority() || !Result.IsSuccess())
	{
		return Result.IsSuccess() ? FGolemResult::Failure(EGolemError::NotAuthority,
			TEXT("QuestRestore"), FText::GetEmpty()) : Result;
	}
	Snapshot = Candidate;
	Snapshot.DefinitionId = Campaign->GetPrimaryAssetId();
	Snapshot.ContentRevision = Campaign->ContentRevision;
	for (const FGolemQuestDefinition& Definition : Campaign->Quests)
	{
		if (FindProgress(Definition.QuestId) == nullptr)
		{
			FGolemQuestProgress& Row = Snapshot.Quests.AddDefaulted_GetRef();
			Row.QuestId = Definition.QuestId;
			for (const FGolemObjectiveDefinition& Objective : Definition.Objectives)
			{
				Row.Counts.Add(Objective.ObjectiveId, 0);
			}
		}
	}
	RefreshAvailability(false);
	PublishSnapshot();
	return FGolemResult::Success();
}

void UGolemQuestComponent::PublishSnapshot()
{
	++Snapshot.Revision;
	GetOwner()->ForceNetUpdate();
	OnSnapshotChanged.Broadcast();
}

void UGolemQuestComponent::OnRep_Snapshot()
{
	OnSnapshotChanged.Broadcast();
}

void UGolemQuestComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Subscription.Bus.IsValid())
	{
		Subscription.Bus->Unsubscribe(Subscription);
	}
	Subscription = FGolemEventSubscription();
	Super::EndPlay(Reason);
}
