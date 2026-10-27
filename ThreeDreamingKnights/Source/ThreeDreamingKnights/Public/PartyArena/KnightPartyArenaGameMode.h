#pragma once

#include "CoreMinimal.h"
#include "Game/KnightGameMode.h"
#include "PartyArena/KnightPartyArenaState.h"
#include "KnightPartyArenaGameMode.generated.h"

class AKnightArenaEnemy;
class UKnightLifeComponent;
struct FKnightBossPhaseSnapshot;

struct FKnightArenaSpawn
{
	FPrimaryAssetId ArchetypeId;
	FPrimaryAssetId BossId;
	FVector Position = FVector::ZeroVector;
};

struct FKnightArenaLifeBinding
{
	TWeakObjectPtr<UKnightLifeComponent> Life;
	FDelegateHandle Handle;
};

UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightPartyArenaGameMode : public AKnightGameMode
{
	GENERATED_BODY()

public:
	AKnightPartyArenaGameMode();
	virtual void InitGameState() override;
	virtual FKnightResult TryInteract(APlayerController* Player) override;
	virtual FKnightResult TryRestartDrill(APlayerController* Player) override;
	AKnightPartyArenaState* GetArenaState() const;
	const FKnightResult& GetArenaInitializationResult() const;
	const TArray<TObjectPtr<AKnightArenaEnemy>>& GetEncounterEnemies() const;

protected:
	/** Fixture adapters may split the existing wave/boss into independently claimable encounters. */
	virtual bool ShouldAdvanceWaveToBoss() const;
	virtual TArray<FKnightArenaSpawn> GetEncounterSpawns(EKnightArenaStage Stage) const;
	FKnightResult StartFixtureEncounter(EKnightArenaStage Stage, bool bElite = false);
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	FKnightResult SpawnEncounter(EKnightArenaStage Stage);
	void ClearEncounter();
	void RefreshEncounter();
	void ScheduleEncounterRefresh();
	void HandleBossPhaseChanged(const FKnightBossPhaseSnapshot& Phase);
	UFUNCTION()
	void HandleEncounterEnemyEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
	UPROPERTY(Transient)
	TObjectPtr<AKnightPartyArenaState> ArenaState;
	UPROPERTY(Transient)
	TArray<TObjectPtr<AKnightArenaEnemy>> Enemies;
	TArray<FKnightArenaLifeBinding> PartyBindings;
	TArray<FKnightArenaLifeBinding> EnemyBindings;
	FDelegateHandle BossPhaseHandle;
	FTimerHandle RefreshTimer;
	FKnightResult ArenaInitializationResult;
	bool bResetting = false;
	bool bEndingPlay = false;
	bool bUnexpectedEnemyLoss = false;
	bool bFixtureElite = false;
};
