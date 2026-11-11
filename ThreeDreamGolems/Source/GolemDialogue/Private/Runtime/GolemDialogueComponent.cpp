#include "Runtime/GolemDialogueComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UGolemDialogueComponent::UGolemDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGolemDialogueComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGolemDialogueComponent, Snapshot);
}

FGolemResult UGolemDialogueComponent::StartConversation(UGolemConversationDefinition* Definition)
{
	if (!GetOwner()->HasAuthority() || Definition == nullptr || !Definition->ValidateConversation().IsSuccess()
		|| Snapshot.bActive)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("ConversationStart"), FText::GetEmpty());
	}
	Conversation = Definition;
	Snapshot.ConversationId = Definition->GetPrimaryAssetId();
	Snapshot.NpcId = Definition->NpcId;
	Snapshot.NodeId = Definition->FirstNodeId;
	Snapshot.bActive = true;
	Snapshot.Text = Definition->FindNode(Snapshot.NodeId)->Text;
	PublishSnapshot();
	return FGolemResult::Success();
}

FGolemResult UGolemDialogueComponent::AdvanceConversation(const bool bHost, const int64 ExpectedRevision,
	const FName ChoiceId, bool& bOutFinished)
{
	bOutFinished = false;
	if (!GetOwner()->HasAuthority() || !bHost || !Snapshot.bActive || Conversation == nullptr
		|| Snapshot.Revision != ExpectedRevision)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("ConversationHost"),
			NSLOCTEXT("GolemDialogue", "HostDecision", "The host advances the shared conversation."));
	}
	const FGolemDialogueNode* Node = Conversation->FindNode(Snapshot.NodeId);
	FName Next = Node->NextNodeId;
	if (!Node->Choices.IsEmpty())
	{
		const FName* Choice = Node->Choices.Find(ChoiceId);
		if (Choice == nullptr)
		{
			return FGolemResult::Failure(EGolemError::InvalidId, ChoiceId, FText::GetEmpty());
		}
		const FName BranchKey(*FString::Printf(TEXT("%s_%s"),
			*Conversation->GetDefinitionId().ToString(), *Node->NodeId.ToString()));
		if (const FName* Previous = Snapshot.Choices.Find(BranchKey))
		{
			if (*Previous != ChoiceId)
			{
				return FGolemResult::Failure(EGolemError::Conflict, BranchKey, FText::GetEmpty());
			}
		}
		Snapshot.Choices.Add(BranchKey, ChoiceId);
		Next = *Choice;
	}
	else if (!ChoiceId.IsNone())
	{
		return FGolemResult::Failure(EGolemError::InvalidId, ChoiceId, FText::GetEmpty());
	}
	Snapshot.NodeId = Next;
	Snapshot.bActive = !Next.IsNone();
	Snapshot.Text = Snapshot.bActive ? Conversation->FindNode(Next)->Text : FText::GetEmpty();
	bOutFinished = !Snapshot.bActive;
	PublishSnapshot();
	return FGolemResult::Success();
}

void UGolemDialogueComponent::CancelConversation()
{
	if (GetOwner()->HasAuthority() && Snapshot.bActive)
	{
		Snapshot.bActive = false;
		Snapshot.NodeId = NAME_None;
		Snapshot.Text = FText::GetEmpty();
		PublishSnapshot();
	}
}

FGolemResult UGolemDialogueComponent::RestoreChoices(const FGolemDialogueSnapshot& Candidate)
{
	if (!GetOwner()->HasAuthority() || Candidate.bActive || Candidate.Revision < 1)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("DialogueCheckpoint"), FText::GetEmpty());
	}
	Snapshot = Candidate;
	Conversation = nullptr;
	PublishSnapshot();
	return FGolemResult::Success();
}

const FGolemDialogueSnapshot& UGolemDialogueComponent::GetSnapshot() const
{
	return Snapshot;
}

void UGolemDialogueComponent::PublishSnapshot()
{
	++Snapshot.Revision;
	GetOwner()->ForceNetUpdate();
	OnSnapshotChanged.Broadcast();
}

void UGolemDialogueComponent::OnRep_Snapshot()
{
	OnSnapshotChanged.Broadcast();
}
