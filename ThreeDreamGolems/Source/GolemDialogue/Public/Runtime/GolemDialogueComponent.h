#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Definitions/GolemConversationDefinition.h"
#include "Types/GolemStoryValues.h"
#include "GolemDialogueComponent.generated.h"

USTRUCT()
struct GOLEMDIALOGUE_API FGolemDialogueSnapshot
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FPrimaryAssetId ConversationId;
	UPROPERTY(SaveGame)
	FName NodeId;
	UPROPERTY(SaveGame)
	FName NpcId;
	UPROPERTY(SaveGame)
	int64 Revision = 1;
	UPROPERTY(SaveGame)
	bool bActive = false;
	UPROPERTY(SaveGame)
	FGolemStoryChoiceMap Choices;
	UPROPERTY()
	FText Text;
};

DECLARE_MULTICAST_DELEGATE(FOnGolemDialogueChanged);

UCLASS()
class GOLEMDIALOGUE_API UGolemDialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGolemDialogueComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGolemResult StartConversation(UGolemConversationDefinition* Definition);
	FGolemResult AdvanceConversation(bool bHost, int64 ExpectedRevision, FName ChoiceId, bool& bOutFinished);
	void CancelConversation();
	FGolemResult RestoreChoices(const FGolemDialogueSnapshot& Snapshot);
	const FGolemDialogueSnapshot& GetSnapshot() const;
	FOnGolemDialogueChanged OnSnapshotChanged;

private:
	void PublishSnapshot();
	UFUNCTION()
	void OnRep_Snapshot();
	UPROPERTY(Transient)
	TObjectPtr<UGolemConversationDefinition> Conversation;
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FGolemDialogueSnapshot Snapshot;
};
