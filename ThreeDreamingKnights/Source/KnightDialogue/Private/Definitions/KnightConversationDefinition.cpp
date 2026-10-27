#include "Definitions/KnightConversationDefinition.h"
#include "Types/KnightStableIds.h"

const FKnightDialogueNode* UKnightConversationDefinition::FindNode(const FName NodeId) const
{
	return Nodes.FindByPredicate([NodeId](const FKnightDialogueNode& Node)
	{
		return Node.NodeId == NodeId;
	});
}

FKnightResult UKnightConversationDefinition::ValidateConversation() const
{
	if (!ValidateIdentity().IsSuccess() || !KnightIdentity::IsValidIdentifier(NpcId) || FindNode(FirstNodeId) == nullptr)
	{
		return FKnightResult::Failure(EKnightError::InvalidData, NpcId, FText::GetEmpty());
	}
	TSet<FName> Ids;
	for (const FKnightDialogueNode& Node : Nodes)
	{
		if (!KnightIdentity::IsValidIdentifier(Node.NodeId) || Ids.Contains(Node.NodeId) || Node.Text.IsEmpty()
			|| !Node.NextNodeId.IsNone() && FindNode(Node.NextNodeId) == nullptr)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, Node.NodeId, FText::GetEmpty());
		}
		Ids.Add(Node.NodeId);
		for (const TPair<FName, FName>& Choice : Node.Choices)
		{
			if (!KnightIdentity::IsValidIdentifier(Choice.Key) || FindNode(Choice.Value) == nullptr)
			{
				return FKnightResult::Failure(EKnightError::NotFound, Choice.Value, FText::GetEmpty());
			}
		}
	}
	return FKnightResult::Success();
}
