#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Definitions/KnightConversationDefinition.h"
#include "Types/KnightStoryValues.h"
#include "KnightDialogueComponent.generated.h"

USTRUCT()
struct KNIGHTDIALOGUE_API FKnightDialogueSnapshot
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
	FKnightStoryChoiceMap Choices;
	UPROPERTY()
	FText Text;
};

DECLARE_MULTICAST_DELEGATE(FOnKnightDialogueChanged);

UCLASS()
class KNIGHTDIALOGUE_API UKnightDialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKnightDialogueComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FKnightResult StartConversation(UKnightConversationDefinition* Definition);
	FKnightResult AdvanceConversation(bool bHost, int64 ExpectedRevision, FName ChoiceId, bool& bOutFinished);
	void CancelConversation();
	FKnightResult RestoreChoices(const FKnightDialogueSnapshot& Snapshot);
	const FKnightDialogueSnapshot& GetSnapshot() const;
	FOnKnightDialogueChanged OnSnapshotChanged;

private:
	void PublishSnapshot();
	UFUNCTION()
	void OnRep_Snapshot();
	UPROPERTY(Transient)
	TObjectPtr<UKnightConversationDefinition> Conversation;
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FKnightDialogueSnapshot Snapshot;
};
