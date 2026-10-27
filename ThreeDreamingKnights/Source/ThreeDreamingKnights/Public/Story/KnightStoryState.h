#pragma once

#include "CoreMinimal.h"
#include "RPGLoop/KnightRPGGameState.h"
#include "Runtime/KnightDialogueComponent.h"
#include "Runtime/KnightQuestComponent.h"
#include "Runtime/KnightStoryWorldComponent.h"
#include "KnightStoryState.generated.h"

class UKnightStoryProfile;

UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightStoryState : public AKnightRPGGameState
{
	GENERATED_BODY()

public:
	AKnightStoryState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void SetWorldDefinitionId(const FPrimaryAssetId& Id);
	FPrimaryAssetId GetWorldDefinitionId() const;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Story")
	TObjectPtr<UKnightQuestComponent> Quest;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Story")
	TObjectPtr<UKnightDialogueComponent> Dialogue;
	UPROPERTY(VisibleAnywhere, Category = "Knight|Story")
	TObjectPtr<UKnightStoryWorldComponent> WorldState;

private:
	UPROPERTY(Replicated)
	FPrimaryAssetId WorldDefinitionId;
};
