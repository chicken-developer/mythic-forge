#pragma once

#include "CoreMinimal.h"
#include "Assets/GolemDefinition.h"
#include "GolemConversationDefinition.generated.h"

USTRUCT()
struct GOLEMDIALOGUE_API FGolemDialogueNode
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
class GOLEMDIALOGUE_API UGolemConversationDefinition : public UGolemDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	FName NpcId;
	UPROPERTY(EditDefaultsOnly)
	FName FirstNodeId;
	UPROPERTY(EditDefaultsOnly)
	TArray<FGolemDialogueNode> Nodes;

	const FGolemDialogueNode* FindNode(FName NodeId) const;
	FGolemResult ValidateConversation() const;
};
