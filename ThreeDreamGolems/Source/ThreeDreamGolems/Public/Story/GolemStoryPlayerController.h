#pragma once

#include "CoreMinimal.h"
#include "RPGLoop/GolemRPGUIPlayerController.h"
#include "GolemStoryPlayerController.generated.h"

class AGolemStoryState;

UCLASS()
class THREEDREAMGOLEMS_API AGolemStoryPlayerController : public AGolemRPGUIPlayerController
{
	GENERATED_BODY()

public:
	void RequestStoryAction(FName Action, FName AnchorId = NAME_None, FName ChoiceId = NAME_None);
	const FText& GetStoryStatus() const;
	const FGolemResult& GetLastStoryResult() const;

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
		FGolemEntityId ExpectedGolem, FGuid RequestId, int64 ExpectedRevision);
	UFUNCTION(Client, Reliable)
	void ClientReceiveStoryResult(FGuid RequestId, FName Action, FGolemResult Result);
	TWeakObjectPtr<AGolemStoryState> Story;
	FDelegateHandle QuestHandle;
	FDelegateHandle WorldHandle;
	FDelegateHandle DialogueHandle;
	FDelegateHandle PresentationHandle;
	FDelegateHandle PanelHandle;
	FDelegateHandle ChoiceHandle;
	FTimerHandle ReadinessTimer;
	FText Status;
	FText Feedback;
	FGolemResult LastResult;
	FGuid PendingRequest;
	int32 BindingAttempts = 0;
	bool bSceneInputOwned = false;
};
