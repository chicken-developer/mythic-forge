#include "Definitions/GolemConversationDefinition.h"
#include "Types/GolemStableIds.h"

const FGolemDialogueNode* UGolemConversationDefinition::FindNode(const FName NodeId) const
{
	return Nodes.FindByPredicate([NodeId](const FGolemDialogueNode& Node)
	{
		return Node.NodeId == NodeId;
	});
}

FGolemResult UGolemConversationDefinition::ValidateConversation() const
{
	if (!ValidateIdentity().IsSuccess() || !GolemIdentity::IsValidIdentifier(NpcId) || FindNode(FirstNodeId) == nullptr)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, NpcId, FText::GetEmpty());
	}
	TSet<FName> Ids;
	for (const FGolemDialogueNode& Node : Nodes)
	{
		if (!GolemIdentity::IsValidIdentifier(Node.NodeId) || Ids.Contains(Node.NodeId) || Node.Text.IsEmpty()
			|| !Node.NextNodeId.IsNone() && FindNode(Node.NextNodeId) == nullptr)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, Node.NodeId, FText::GetEmpty());
		}
		Ids.Add(Node.NodeId);
		for (const TPair<FName, FName>& Choice : Node.Choices)
		{
			if (!GolemIdentity::IsValidIdentifier(Choice.Key) || FindNode(Choice.Value) == nullptr)
			{
				return FGolemResult::Failure(EGolemError::NotFound, Choice.Value, FText::GetEmpty());
			}
		}
	}
	return FGolemResult::Success();
}
