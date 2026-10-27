#pragma once

#include "CoreMinimal.h"
#include "Checkpoint/KnightCheckpointSave.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "KnightCheckpointSubsystem.generated.h"

/** One local save owner. Disk success is acknowledged only after flush, checksum and replacement. */
UCLASS()
class KNIGHTSAVE_API UKnightCheckpointSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	FKnightResult SaveCheckpoint(const FKnightCampaignCheckpoint& Checkpoint);
	FKnightResult LoadCheckpoint(FName CampaignId, FName ScenarioKind, FKnightCampaignCheckpoint& OutCheckpoint);
	FKnightResult StageCheckpoint(const FKnightCampaignCheckpoint& Checkpoint);
	const FKnightCampaignCheckpoint* GetStagedCheckpoint() const;
	void ConsumeStagedCheckpoint();
	static FString GetCheckpointPath(FName CampaignId, FName ScenarioKind);
	/** Injectable filesystem boundary for native crash/recovery tests, never a gameplay command. */
	static FKnightResult WriteCheckpointFile(const FString& Path, const FKnightCampaignCheckpoint& Checkpoint,
		bool bInterruptBeforeReplace = false);
	static FKnightResult ReadCheckpointFile(const FString& Path, FKnightCampaignCheckpoint& OutCheckpoint);

private:
	UPROPERTY(Transient)
	FKnightCampaignCheckpoint Staged;
	bool bHasStagedCheckpoint = false;
};
