#pragma once

#include "CoreMinimal.h"
#include "Checkpoint/GolemCheckpointSave.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GolemCheckpointSubsystem.generated.h"

/** One local save owner. Disk success is acknowledged only after flush, checksum and replacement. */
UCLASS()
class GOLEMSAVE_API UGolemCheckpointSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	FGolemResult SaveCheckpoint(const FGolemCampaignCheckpoint& Checkpoint);
	FGolemResult LoadCheckpoint(FName CampaignId, FName ScenarioKind, FGolemCampaignCheckpoint& OutCheckpoint);
	FGolemResult StageCheckpoint(const FGolemCampaignCheckpoint& Checkpoint);
	const FGolemCampaignCheckpoint* GetStagedCheckpoint() const;
	void ConsumeStagedCheckpoint();
	static FString GetCheckpointPath(FName CampaignId, FName ScenarioKind);
	/** Injectable filesystem boundary for native crash/recovery tests, never a gameplay command. */
	static FGolemResult WriteCheckpointFile(const FString& Path, const FGolemCampaignCheckpoint& Checkpoint,
		bool bInterruptBeforeReplace = false);
	static FGolemResult ReadCheckpointFile(const FString& Path, FGolemCampaignCheckpoint& OutCheckpoint);

private:
	UPROPERTY(Transient)
	FGolemCampaignCheckpoint Staged;
	bool bHasStagedCheckpoint = false;
};
