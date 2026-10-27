#include "Runtime/KnightQuestComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UKnightQuestComponent::UKnightQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UKnightQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKnightQuestComponent, Snapshot);
}

FKnightResult UKnightQuestComponent::InitializeCampaign(UKnightCampaignDefinition* Definition, const FGuid& RunId)
{
	if (!GetOwner()->HasAuthority() || Campaign != nullptr || Definition == nullptr
		|| !RunId.IsValid() || !Definition->ValidateCampaign().IsSuccess())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("CampaignInitialize"), FText::GetEmpty());
	}
	Campaign = Definition;
	Snapshot.DefinitionId = Campaign->GetPrimaryAssetId();
	Snapshot.CampaignId = Campaign->CampaignId;
	Snapshot.ScenarioKind = Campaign->ScenarioKind;
	Snapshot.ContentRevision = Campaign->ContentRevision;
	Snapshot.RunId = RunId;
	for (const FKnightQuestDefinition& DefinitionRow : Campaign->Quests)
	{
		FKnightQuestProgress& Row = Snapshot.Quests.AddDefaulted_GetRef();
		Row.QuestId = DefinitionRow.QuestId;
		for (const FKnightObjectiveDefinition& Objective : DefinitionRow.Objectives)
		{
			Row.Counts.Add(Objective.ObjectiveId, 0);
		}
	}
	RefreshAvailability(true);
	Subscription = GetWorld()->GetSubsystem<UKnightEventSubsystem>()->Subscribe<FKnightStoryEvent>(this,
		[this](const FKnightStoryEvent& Fact)
		{
			HandleFact(Fact);
		});
	PublishSnapshot();
	return FKnightResult::Success();
}

const FKnightQuestSnapshot& UKnightQuestComponent::GetSnapshot() const
{
	return Snapshot;
}

const FKnightQuestProgress* UKnightQuestComponent::FindProgress(const FName QuestId) const
{
	return Snapshot.Quests.FindByPredicate([QuestId](const FKnightQuestProgress& Row)
	{
		return Row.QuestId == QuestId;
	});
}

void UKnightQuestComponent::RefreshAvailability(const bool bActivateAutomatically)
{
	for (FKnightQuestProgress& Row : Snapshot.Quests)
	{
		if (Row.Phase != EKnightQuestPhase::Locked)
		{
			continue;
		}
		const FKnightQuestDefinition* Definition = Campaign->FindQuest(Row.QuestId);
		bool bReady = true;
		for (const FName Parent : Definition->Prerequisites)
		{
			const FKnightQuestProgress* Progress = FindProgress(Parent);
			bReady &= Progress != nullptr && Progress->Phase == EKnightQuestPhase::Complete;
		}
		if (bReady)
		{
			Row.Phase = bActivateAutomatically && Definition->bAutoActivate
				? EKnightQuestPhase::Active : EKnightQuestPhase::Available;
			Row.ActivatedRevision = Snapshot.FactRevision;
		}
	}
}

FKnightResult UKnightQuestComponent::ActivateQuest(const FName QuestId)
{
	FKnightQuestProgress* Row = Snapshot.Quests.FindByPredicate([QuestId](const FKnightQuestProgress& Entry)
	{
		return Entry.QuestId == QuestId;
	});
	if (!GetOwner()->HasAuthority() || Row == nullptr || Row->Phase != EKnightQuestPhase::Available)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, QuestId, FText::GetEmpty());
	}
	Row->Phase = EKnightQuestPhase::Active;
	Row->ActivatedRevision = Snapshot.FactRevision;
	PublishSnapshot();
	return FKnightResult::Success();
}

void UKnightQuestComponent::HandleFact(const FKnightStoryEvent& Fact)
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

bool UKnightQuestComponent::ApplyFact(const FKnightStoryEvent& Fact, const bool bCurrentState)
{
	bool bChanged = false;
	for (FKnightQuestProgress& Row : Snapshot.Quests)
	{
		if (Row.Phase != EKnightQuestPhase::Active)
		{
			continue;
		}
		const FKnightQuestDefinition* Definition = Campaign->FindQuest(Row.QuestId);
		for (const FKnightObjectiveDefinition& Objective : Definition->Objectives)
		{
			int32& Count = Row.Counts.FindChecked(Objective.ObjectiveId);
			if (Count >= Objective.RequiredCount)
			{
				continue;
			}
			const bool bMatches = Objective.EventName == Fact.EventName && Objective.SubjectId == Fact.SubjectId
				&& (Objective.ValueId.IsNone() || Objective.ValueId == Fact.ValueId)
				&& (bCurrentState ? Objective.Policy == EKnightObjectivePolicy::CurrentState
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
		for (const FKnightObjectiveDefinition& Objective : Definition->Objectives)
		{
			bComplete &= Row.Counts.FindChecked(Objective.ObjectiveId) == Objective.RequiredCount;
		}
		if (bComplete)
		{
			Row.Phase = Definition->RewardKey.IsNone() ? EKnightQuestPhase::Complete : EKnightQuestPhase::RewardPending;
		}
	}
	RefreshAvailability(true);
	return bChanged;
}

FKnightResult UKnightQuestComponent::ApplyCurrentFact(const FKnightStoryEvent& Fact)
{
	if (!GetOwner()->HasAuthority() || Fact.CampaignId != Snapshot.CampaignId || Fact.RunId != Snapshot.RunId)
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, Fact.SubjectId, FText::GetEmpty());
	}
	if (ApplyFact(Fact, true))
	{
		PublishSnapshot();
	}
	return FKnightResult::Success();
}

FKnightResult UKnightQuestComponent::CommitReward(const FName QuestId, const FGuid& Receipt)
{
	FKnightQuestProgress* Row = Snapshot.Quests.FindByPredicate([QuestId](const FKnightQuestProgress& Entry)
	{
		return Entry.QuestId == QuestId;
	});
	if (!GetOwner()->HasAuthority() || Row == nullptr || !Receipt.IsValid()
		|| (Row->Phase != EKnightQuestPhase::RewardPending && Row->RewardReceipt != Receipt))
	{
		return FKnightResult::Failure(EKnightError::InvalidState, QuestId, FText::GetEmpty());
	}
	if (Row->Phase == EKnightQuestPhase::Complete)
	{
		return FKnightResult::Success();
	}
	Row->RewardReceipt = Receipt;
	Row->Phase = EKnightQuestPhase::Complete;
	RefreshAvailability(true);
	PublishSnapshot();
	return FKnightResult::Success();
}

FKnightResult UKnightQuestComponent::CommitBranch(const FName BranchId, const FName ChoiceId,
	const int64 ExpectedRevision)
{
	if (!GetOwner()->HasAuthority() || !KnightIdentity::IsValidIdentifier(BranchId)
		|| !KnightIdentity::IsValidIdentifier(ChoiceId) || ExpectedRevision != Snapshot.Revision)
	{
		return FKnightResult::Failure(EKnightError::InvalidRevision, BranchId, FText::GetEmpty());
	}
	if (const FName* Previous = Snapshot.Branches.Find(BranchId))
	{
		return *Previous == ChoiceId ? FKnightResult::Success()
			: FKnightResult::Failure(EKnightError::Conflict, BranchId, FText::GetEmpty());
	}
	Snapshot.Branches.Add(BranchId, ChoiceId);
	PublishSnapshot();
	return FKnightResult::Success();
}

FKnightResult UKnightQuestComponent::SetEncounterRun(const FGuid& EncounterRunId)
{
	if (!GetOwner()->HasAuthority() || !EncounterRunId.IsValid())
	{
		return FKnightResult::Failure(EKnightError::InvalidId, TEXT("EncounterGeneration"), FText::GetEmpty());
	}
	EncounterRun = EncounterRunId;
	return FKnightResult::Success();
}

FKnightResult UKnightQuestComponent::ValidateSnapshot(const FKnightQuestSnapshot& Candidate) const
{
	if (Campaign == nullptr || Candidate.CampaignId != Campaign->CampaignId
		|| Candidate.ScenarioKind != Campaign->ScenarioKind || !Candidate.RunId.IsValid()
		|| Candidate.Revision < 1 || Candidate.FactRevision < 0
		|| Candidate.ContentRevision < 1 || Candidate.ContentRevision > Campaign->ContentRevision)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("QuestSnapshotScope"), FText::GetEmpty());
	}
	TSet<FName> Ids;
	TSet<FGuid> Facts;
	for (const FGuid Id : Candidate.SeenFacts)
	{
		if (!Id.IsValid() || Facts.Contains(Id))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("QuestFacts"), FText::GetEmpty());
		}
		Facts.Add(Id);
	}
	for (const FKnightQuestProgress& Row : Candidate.Quests)
	{
		const FKnightQuestDefinition* Definition = Campaign->FindQuest(Row.QuestId);
		if (Definition == nullptr || Ids.Contains(Row.QuestId) || Row.Counts.Num() != Definition->Objectives.Num()
			|| Row.ActivatedRevision < 0 || Row.ActivatedRevision > Candidate.FactRevision
			|| Row.Phase > EKnightQuestPhase::Complete)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, Row.QuestId, FText::GetEmpty());
		}
		Ids.Add(Row.QuestId);
		bool bFinished = true;
		bool bFoundIncomplete = false;
		for (const FKnightObjectiveDefinition& Objective : Definition->Objectives)
		{
			const int32* Count = Row.Counts.Find(Objective.ObjectiveId);
			if (Count == nullptr || *Count < 0 || *Count > Objective.RequiredCount
				|| Definition->bOrdered && bFoundIncomplete && *Count > 0)
			{
				return FKnightResult::Failure(EKnightError::InvalidData, Objective.ObjectiveId, FText::GetEmpty());
			}
			bFinished &= *Count == Objective.RequiredCount;
			bFoundIncomplete |= *Count < Objective.RequiredCount;
		}
		if ((Row.Phase >= EKnightQuestPhase::RewardPending && !bFinished)
			|| (Row.Phase == EKnightQuestPhase::Complete && !Definition->RewardKey.IsNone()
				&& !Row.RewardReceipt.IsValid()))
		{
			return FKnightResult::Failure(EKnightError::InvalidData, Row.QuestId, FText::GetEmpty());
		}
	}
	for (const FKnightQuestDefinition& Definition : Campaign->Quests)
	{
		if (Definition.IntroducedRevision <= Candidate.ContentRevision && !Ids.Contains(Definition.QuestId))
		{
			return FKnightResult::Failure(EKnightError::NotFound, Definition.QuestId, FText::GetEmpty());
		}
	}
	// Removed content is an error. Additions are deliberately allowed and become Available on load.
	return FKnightResult::Success();
}

FKnightResult UKnightQuestComponent::RestoreSnapshot(const FKnightQuestSnapshot& Candidate)
{
	FKnightResult Result = ValidateSnapshot(Candidate);
	if (!GetOwner()->HasAuthority() || !Result.IsSuccess())
	{
		return Result.IsSuccess() ? FKnightResult::Failure(EKnightError::NotAuthority,
			TEXT("QuestRestore"), FText::GetEmpty()) : Result;
	}
	Snapshot = Candidate;
	Snapshot.DefinitionId = Campaign->GetPrimaryAssetId();
	Snapshot.ContentRevision = Campaign->ContentRevision;
	for (const FKnightQuestDefinition& Definition : Campaign->Quests)
	{
		if (FindProgress(Definition.QuestId) == nullptr)
		{
			FKnightQuestProgress& Row = Snapshot.Quests.AddDefaulted_GetRef();
			Row.QuestId = Definition.QuestId;
			for (const FKnightObjectiveDefinition& Objective : Definition.Objectives)
			{
				Row.Counts.Add(Objective.ObjectiveId, 0);
			}
		}
	}
	RefreshAvailability(false);
	PublishSnapshot();
	return FKnightResult::Success();
}

void UKnightQuestComponent::PublishSnapshot()
{
	++Snapshot.Revision;
	GetOwner()->ForceNetUpdate();
	OnSnapshotChanged.Broadcast();
}

void UKnightQuestComponent::OnRep_Snapshot()
{
	OnSnapshotChanged.Broadcast();
}

void UKnightQuestComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Subscription.Bus.IsValid())
	{
		Subscription.Bus->Unsubscribe(Subscription);
	}
	Subscription = FKnightEventSubscription();
	Super::EndPlay(Reason);
}
