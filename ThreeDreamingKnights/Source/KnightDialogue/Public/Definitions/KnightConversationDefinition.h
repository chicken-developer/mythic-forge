#pragma once

#include "CoreMinimal.h"
#include "Assets/KnightDefinition.h"
#include "KnightConversationDefinition.generated.h"

USTRUCT()
struct KNIGHTDIALOGUE_API FKnightDialogueNode
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	FName NodeId;
	UPROPERTY(EditDefaultsOnly)
	FText Text;
	UPROPERTY(EditDefaultsOnly)
	FName NextNodeId;
	UPROPERTY(EditDefaultsOnly)
	TMap<FName, FName> Choices;
};

UCLASS()
class KNIGHTDIALOGUE_API UKnightConversationDefinition : public UKnightDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	FName NpcId;
	UPROPERTY(EditDefaultsOnly)
	FName FirstNodeId;
	UPROPERTY(EditDefaultsOnly)
	TArray<FKnightDialogueNode> Nodes;

	const FKnightDialogueNode* FindNode(FName NodeId) const;
	FKnightResult ValidateConversation() const;
};
