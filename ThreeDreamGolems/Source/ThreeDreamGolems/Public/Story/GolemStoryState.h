#pragma once

#include "CoreMinimal.h"
#include "RPGLoop/GolemRPGGameState.h"
#include "Runtime/GolemDialogueComponent.h"
#include "Runtime/GolemQuestComponent.h"
#include "Runtime/GolemStoryWorldComponent.h"
#include "GolemStoryState.generated.h"

class UGolemStoryProfile;

UCLASS()
class THREEDREAMGOLEMS_API AGolemStoryState : public AGolemRPGGameState
{
	GENERATED_BODY()

public:
	AGolemStoryState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void SetWorldDefinitionId(const FPrimaryAssetId& Id);
	FPrimaryAssetId GetWorldDefinitionId() const;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Story")
	TObjectPtr<UGolemQuestComponent> Quest;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Story")
	TObjectPtr<UGolemDialogueComponent> Dialogue;
	UPROPERTY(VisibleAnywhere, Category = "Golem|Story")
	TObjectPtr<UGolemStoryWorldComponent> WorldState;

private:
	UPROPERTY(Replicated)
	FPrimaryAssetId WorldDefinitionId;
};
