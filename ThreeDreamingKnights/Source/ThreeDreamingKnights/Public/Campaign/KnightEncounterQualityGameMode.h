#pragma once

#include "CoreMinimal.h"
#include "Campaign/KnightCampaignGameMode.h"
#include "KnightEncounterQualityGameMode.generated.h"

class UKnightEncounterQualityDefinition;
struct FKnightPartyArenaSnapshot;

/** Development encounter uses campaign UI, transactions and the original StateTree/GAS combat. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightEncounterQualityGameMode : public AKnightCampaignGameMode
{
	GENERATED_BODY()

public:
	virtual FKnightResult TryInteract(APlayerController* Player) override;
	virtual FKnightResult TryRestartDrill(APlayerController* Player) override;
	FKnightResult TrySetDifficulty(APlayerController* Player, FName Difficulty);
	FName GetDifficulty() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual bool ShouldAdvanceWaveToBoss() const override;
	virtual TArray<FKnightArenaSpawn> GetEncounterSpawns(EKnightArenaStage Stage) const override;

private:
	void HandleQualityArenaChanged(const FKnightPartyArenaSnapshot& Snapshot);
	UPROPERTY(Transient)
	TObjectPtr<UKnightEncounterQualityDefinition> Encounter;
	FDelegateHandle QualityArenaHandle;
	FName DifficultyId = TEXT("Standard");
};
