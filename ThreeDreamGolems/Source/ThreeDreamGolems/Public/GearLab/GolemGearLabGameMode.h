#pragma once

#include "CoreMinimal.h"
#include "SkillLab/GolemSkillLabGameMode.h"
#include "GolemGearLabGameMode.generated.h"

class AGolemGearLabState;
class UGolemAbilitySystemComponent;

/** Isolated itemization fixture built on the playable M4/M5/M5.5 domains. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemGearLabGameMode : public AGolemSkillLabGameMode
{
	GENERATED_BODY()
public:
	AGolemGearLabGameMode();
	virtual void InitGameState() override;
	virtual FGolemResult TryInteract(APlayerController* Player) override;
	virtual FGolemResult TryRestartDrill(APlayerController* Player) override;
	virtual FGolemRPGCommandResult ExecuteRPGCommand(AGolemRPGPlayerController* Player,
		const FGolemRPGCommand& Command) override;
	FGolemResult ExecuteFixtureAction(APlayerController* Player, FName Action, const FGuid& RequestId);
	AGolemGearLabState* GetGearLabState() const;
protected:
	virtual bool IsCatalogLab() const;
	virtual FGolemEntityId CreateGolemIdentity(int32 Slot, const FPrimaryAssetId& HeroId) const override;
	virtual FGolemResult InitializeRPGDomains(UGolemInventoryComponent* Inventory, UGolemRPGComponent* RPG,
		TConstArrayView<FGolemRPGGolemRegistration> Registrations) override;
	virtual int32 GetInitialInventoryCapacity() const override;
	virtual bool ShouldIssueLegacyEncounterRewards() const override;
	virtual bool ShouldAdvanceWaveToBoss() const override;
	virtual FGolemResult BuildCaller(APlayerController* Player, FGolemRPGCallerContext& OutCaller) const override;
	virtual FGolemResult ConfigureRPGDomains(UGolemInventoryComponent* Inventory, UGolemRPGComponent* RPG) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	void HandleGearArenaChanged(const FGolemPartyArenaSnapshot& Snapshot);
	void RefreshFixture();
	FGolemResult StartGearEncounter(APlayerController* Player);
	void GuardSnapshotTravelAbilities();
	void ReleaseSnapshotTravelAbilities();
	UPROPERTY(Transient)
	TObjectPtr<AGolemGearLabState> GearState;
	FGuid DemoRunId;
	FGuid PendingRewardId;
	int32 CompletedEncounters = 0;
	int32 ActiveEncounter = INDEX_NONE;
	bool bFixtureTravelPending = false;
	TArray<TWeakObjectPtr<UGolemAbilitySystemComponent>> SnapshotTravelAbilitySystems;
	FDelegateHandle GearArenaHandle;
	/** Fixture tuning only; gear, damage formulas and specialization coefficients remain unchanged. */
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Gear|Fixture", meta = (ClampMin = "100"))
	float WaveEnemyHealth = 500.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Gear|Fixture", meta = (ClampMin = "100"))
	float EliteEnemyHealth = 1200.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Golem|Gear|Fixture", meta = (ClampMin = "100"))
	float BossEnemyHealth = 5000.0f;
	TMap<FGuid, FName> FixtureRequests;
	TMap<FGuid, FGolemResult> FixtureResults;
};
