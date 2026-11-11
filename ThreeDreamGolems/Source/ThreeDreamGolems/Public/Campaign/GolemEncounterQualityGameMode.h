#pragma once

#include "CoreMinimal.h"
#include "Campaign/GolemCampaignGameMode.h"
#include "GolemEncounterQualityGameMode.generated.h"

class UGolemEncounterQualityDefinition;
struct FGolemPartyArenaSnapshot;

/** Development encounter uses campaign UI, transactions and the original StateTree/GAS combat. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemEncounterQualityGameMode : public AGolemCampaignGameMode
{
	GENERATED_BODY()

public:
	virtual FGolemResult TryInteract(APlayerController* Player) override;
	virtual FGolemResult TryRestartDrill(APlayerController* Player) override;
	FGolemResult TrySetDifficulty(APlayerController* Player, FName Difficulty);
	FName GetDifficulty() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual bool ShouldAdvanceWaveToBoss() const override;
	virtual TArray<FGolemArenaSpawn> GetEncounterSpawns(EGolemArenaStage Stage) const override;

private:
	void HandleQualityArenaChanged(const FGolemPartyArenaSnapshot& Snapshot);
	UPROPERTY(Transient)
	TObjectPtr<UGolemEncounterQualityDefinition> Encounter;
	FDelegateHandle QualityArenaHandle;
	FName DifficultyId = TEXT("Standard");
};
