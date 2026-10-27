#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Models/KnightRPGModels.h"
#include "Runtime/KnightDialogueComponent.h"
#include "Runtime/KnightQuestComponent.h"
#include "Runtime/KnightStoryWorldComponent.h"
#include "KnightCheckpointSave.generated.h"

USTRUCT()
struct KNIGHTSAVE_API FKnightCampaignCheckpoint
{
	GENERATED_BODY()

	// Existing Items snapshot remains schema 3; this is its first disk envelope, version 1.
	UPROPERTY(SaveGame)
	int32 EnvelopeVersion = 1;
	UPROPERTY(SaveGame)
	FKnightRPGSnapshot RPG;
	UPROPERTY(SaveGame)
	FKnightQuestSnapshot Quest;
	UPROPERTY(SaveGame)
	FKnightStoryWorldSnapshot World;
	UPROPERTY(SaveGame)
	FKnightDialogueSnapshot Dialogue;
	UPROPERTY(SaveGame)
	TMap<FKnightEntityId, float> Poise;
	UPROPERTY(SaveGame)
	FString MapPackage;
	UPROPERTY(SaveGame)
	FGuid CheckpointId;

	FKnightResult ValidateScope() const;
};

UCLASS()
class KNIGHTSAVE_API UKnightCheckpointSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	FKnightCampaignCheckpoint Checkpoint;
};
