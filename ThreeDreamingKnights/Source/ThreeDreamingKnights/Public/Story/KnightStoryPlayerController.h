#pragma once

#include "CoreMinimal.h"
#include "RPGLoop/KnightRPGUIPlayerController.h"
#include "KnightStoryPlayerController.generated.h"

class AKnightStoryState;

UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightStoryPlayerController : public AKnightRPGUIPlayerController
{
	GENERATED_BODY()

public:
	void RequestStoryAction(FName Action, FName AnchorId = NAME_None, FName ChoiceId = NAME_None);
	const FText& GetStoryStatus() const;
	const FKnightResult& GetLastStoryResult() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	void BindStorySources();
	void RefreshStoryPresentation();
	void HandlePanelAction(FName Action);
	void HandlePanelChoice(FName AnchorId, FName ChoiceId);
	UFUNCTION(Server, Reliable)
	void ServerRequestStoryAction(FName Action, FName AnchorId, FName ChoiceId,
		FKnightEntityId ExpectedKnight, FGuid RequestId, int64 ExpectedRevision);
	UFUNCTION(Client, Reliable)
	void ClientReceiveStoryResult(FGuid RequestId, FName Action, FKnightResult Result);
	TWeakObjectPtr<AKnightStoryState> Story;
	FDelegateHandle QuestHandle;
	FDelegateHandle WorldHandle;
	FDelegateHandle DialogueHandle;
	FDelegateHandle PresentationHandle;
	FDelegateHandle PanelHandle;
	FDelegateHandle ChoiceHandle;
	FTimerHandle ReadinessTimer;
	FText Status;
	FText Feedback;
	FKnightResult LastResult;
	FGuid PendingRequest;
	int32 BindingAttempts = 0;
	bool bSceneInputOwned = false;
};
