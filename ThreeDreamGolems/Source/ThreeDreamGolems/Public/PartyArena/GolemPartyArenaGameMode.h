#pragma once

#include "CoreMinimal.h"
#include "Game/GolemGameMode.h"
#include "PartyArena/GolemPartyArenaState.h"
#include "GolemPartyArenaGameMode.generated.h"

class AGolemArenaEnemy;
class UGolemLifeComponent;
struct FGolemBossPhaseSnapshot;

struct FGolemArenaSpawn
{
	FPrimaryAssetId ArchetypeId;
	FPrimaryAssetId BossId;
	FVector Position = FVector::ZeroVector;
};

struct FGolemArenaLifeBinding
{
	TWeakObjectPtr<UGolemLifeComponent> Life;
	FDelegateHandle Handle;
};

UCLASS()
class THREEDREAMGOLEMS_API AGolemPartyArenaGameMode : public AGolemGameMode
{
	GENERATED_BODY()

public:
	AGolemPartyArenaGameMode();
	virtual void InitGameState() override;
	virtual FGolemResult TryInteract(APlayerController* Player) override;
	virtual FGolemResult TryRestartDrill(APlayerController* Player) override;
	AGolemPartyArenaState* GetArenaState() const;
	const FGolemResult& GetArenaInitializationResult() const;
	const TArray<TObjectPtr<AGolemArenaEnemy>>& GetEncounterEnemies() const;

protected:
	/** Fixture adapters may split the existing wave/boss into independently claimable encounters. */
	virtual bool ShouldAdvanceWaveToBoss() const;
	virtual TArray<FGolemArenaSpawn> GetEncounterSpawns(EGolemArenaStage Stage) const;
	FGolemResult StartFixtureEncounter(EGolemArenaStage Stage, bool bElite = false);
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	FGolemResult SpawnEncounter(EGolemArenaStage Stage);
	void ClearEncounter();
	void RefreshEncounter();
	void ScheduleEncounterRefresh();
	void HandleBossPhaseChanged(const FGolemBossPhaseSnapshot& Phase);
	UFUNCTION()
	void HandleEncounterEnemyEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
	UPROPERTY(Transient)
	TObjectPtr<AGolemPartyArenaState> ArenaState;
	UPROPERTY(Transient)
	TArray<TObjectPtr<AGolemArenaEnemy>> Enemies;
	TArray<FGolemArenaLifeBinding> PartyBindings;
	TArray<FGolemArenaLifeBinding> EnemyBindings;
	FDelegateHandle BossPhaseHandle;
	FTimerHandle RefreshTimer;
	FGolemResult ArenaInitializationResult;
	bool bResetting = false;
	bool bEndingPlay = false;
	bool bUnexpectedEnemyLoss = false;
	bool bFixtureElite = false;
};
