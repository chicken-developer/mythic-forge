#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Models/GolemRPGModels.h"
#include "Runtime/GolemDialogueComponent.h"
#include "Runtime/GolemQuestComponent.h"
#include "Runtime/GolemStoryWorldComponent.h"
#include "GolemCheckpointSave.generated.h"

USTRUCT()
struct GOLEMSAVE_API FGolemCampaignCheckpoint
{
	GENERATED_BODY()

	// Existing Items snapshot remains schema 3; this is its first disk envelope, version 1.
	UPROPERTY(SaveGame)
	int32 EnvelopeVersion = 1;
	UPROPERTY(SaveGame)
	FGolemRPGSnapshot RPG;
	UPROPERTY(SaveGame)
	FGolemQuestSnapshot Quest;
	UPROPERTY(SaveGame)
	FGolemStoryWorldSnapshot World;
	UPROPERTY(SaveGame)
	FGolemDialogueSnapshot Dialogue;
	UPROPERTY(SaveGame)
	TMap<FGolemEntityId, float> Poise;
	UPROPERTY(SaveGame)
	FString MapPackage;
	UPROPERTY(SaveGame)
	FGuid CheckpointId;

	FGolemResult ValidateScope() const;
};

UCLASS()
class GOLEMSAVE_API UGolemCheckpointSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	FGolemCampaignCheckpoint Checkpoint;
};
