#include "Runtime/KnightDialogueComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UKnightDialogueComponent::UKnightDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UKnightDialogueComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UKnightDialogueComponent, Snapshot);
}

FKnightResult UKnightDialogueComponent::StartConversation(UKnightConversationDefinition* Definition)
{
	if (!GetOwner()->HasAuthority() || Definition == nullptr || !Definition->ValidateConversation().IsSuccess()
		|| Snapshot.bActive)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("ConversationStart"), FText::GetEmpty());
	}
	Conversation = Definition;
	Snapshot.ConversationId = Definition->GetPrimaryAssetId();
	Snapshot.NpcId = Definition->NpcId;
	Snapshot.NodeId = Definition->FirstNodeId;
	Snapshot.bActive = true;
	Snapshot.Text = Definition->FindNode(Snapshot.NodeId)->Text;
	PublishSnapshot();
	return FKnightResult::Success();
}

FKnightResult UKnightDialogueComponent::AdvanceConversation(const bool bHost, const int64 ExpectedRevision,
	const FName ChoiceId, bool& bOutFinished)
{
	bOutFinished = false;
	if (!GetOwner()->HasAuthority() || !bHost || !Snapshot.bActive || Conversation == nullptr
		|| Snapshot.Revision != ExpectedRevision)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("ConversationHost"),
			NSLOCTEXT("KnightDialogue", "HostDecision", "The host advances the shared conversation."));
	}
	const FKnightDialogueNode* Node = Conversation->FindNode(Snapshot.NodeId);
	FName Next = Node->NextNodeId;
	if (!Node->Choices.IsEmpty())
	{
		const FName* Choice = Node->Choices.Find(ChoiceId);
		if (Choice == nullptr)
		{
			return FKnightResult::Failure(EKnightError::InvalidId, ChoiceId, FText::GetEmpty());
		}
		const FName BranchKey(*FString::Printf(TEXT("%s_%s"),
			*Conversation->GetDefinitionId().ToString(), *Node->NodeId.ToString()));
		if (const FName* Previous = Snapshot.Choices.Find(BranchKey))
		{
			if (*Previous != ChoiceId)
			{
				return FKnightResult::Failure(EKnightError::Conflict, BranchKey, FText::GetEmpty());
			}
		}
		Snapshot.Choices.Add(BranchKey, ChoiceId);
		Next = *Choice;
	}
	else if (!ChoiceId.IsNone())
	{
		return FKnightResult::Failure(EKnightError::InvalidId, ChoiceId, FText::GetEmpty());
	}
	Snapshot.NodeId = Next;
	Snapshot.bActive = !Next.IsNone();
	Snapshot.Text = Snapshot.bActive ? Conversation->FindNode(Next)->Text : FText::GetEmpty();
	bOutFinished = !Snapshot.bActive;
	PublishSnapshot();
	return FKnightResult::Success();
}

void UKnightDialogueComponent::CancelConversation()
{
	if (GetOwner()->HasAuthority() && Snapshot.bActive)
	{
		Snapshot.bActive = false;
		Snapshot.NodeId = NAME_None;
		Snapshot.Text = FText::GetEmpty();
		PublishSnapshot();
	}
}

FKnightResult UKnightDialogueComponent::RestoreChoices(const FKnightDialogueSnapshot& Candidate)
{
	if (!GetOwner()->HasAuthority() || Candidate.bActive || Candidate.Revision < 1)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("DialogueCheckpoint"), FText::GetEmpty());
	}
	Snapshot = Candidate;
	Conversation = nullptr;
	PublishSnapshot();
	return FKnightResult::Success();
}

const FKnightDialogueSnapshot& UKnightDialogueComponent::GetSnapshot() const
{
	return Snapshot;
}

void UKnightDialogueComponent::PublishSnapshot()
{
	++Snapshot.Revision;
	GetOwner()->ForceNetUpdate();
	OnSnapshotChanged.Broadcast();
}

void UKnightDialogueComponent::OnRep_Snapshot()
{
	OnSnapshotChanged.Broadcast();
}
