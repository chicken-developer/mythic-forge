#pragma once

#include "CoreMinimal.h"
#include "SkillLab/KnightSkillLabGameMode.h"
#include "KnightGearLabGameMode.generated.h"

class AKnightGearLabState;
class UKnightAbilitySystemComponent;

/** Isolated itemization fixture built on the playable M4/M5/M5.5 domains. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightGearLabGameMode : public AKnightSkillLabGameMode
{
	GENERATED_BODY()
public:
	AKnightGearLabGameMode();
	virtual void InitGameState() override;
	virtual FKnightResult TryInteract(APlayerController* Player) override;
	virtual FKnightResult TryRestartDrill(APlayerController* Player) override;
	virtual FKnightRPGCommandResult ExecuteRPGCommand(AKnightRPGPlayerController* Player,
		const FKnightRPGCommand& Command) override;
	FKnightResult ExecuteFixtureAction(APlayerController* Player, FName Action, const FGuid& RequestId);
	AKnightGearLabState* GetGearLabState() const;
protected:
	virtual bool IsCatalogLab() const;
	virtual FKnightEntityId CreateKnightIdentity(int32 Slot, const FPrimaryAssetId& HeroId) const override;
	virtual FKnightResult InitializeRPGDomains(UKnightInventoryComponent* Inventory, UKnightRPGComponent* RPG,
		TConstArrayView<FKnightRPGKnightRegistration> Registrations) override;
	virtual int32 GetInitialInventoryCapacity() const override;
	virtual bool ShouldIssueLegacyEncounterRewards() const override;
	virtual bool ShouldAdvanceWaveToBoss() const override;
	virtual FKnightResult BuildCaller(APlayerController* Player, FKnightRPGCallerContext& OutCaller) const override;
	virtual FKnightResult ConfigureRPGDomains(UKnightInventoryComponent* Inventory, UKnightRPGComponent* RPG) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	void HandleGearArenaChanged(const FKnightPartyArenaSnapshot& Snapshot);
	void RefreshFixture();
	FKnightResult StartGearEncounter(APlayerController* Player);
	void GuardSnapshotTravelAbilities();
	void ReleaseSnapshotTravelAbilities();
	UPROPERTY(Transient)
	TObjectPtr<AKnightGearLabState> GearState;
	FGuid DemoRunId;
	FGuid PendingRewardId;
	int32 CompletedEncounters = 0;
	int32 ActiveEncounter = INDEX_NONE;
	bool bFixtureTravelPending = false;
	TArray<TWeakObjectPtr<UKnightAbilitySystemComponent>> SnapshotTravelAbilitySystems;
	FDelegateHandle GearArenaHandle;
	/** Fixture tuning only; gear, damage formulas and specialization coefficients remain unchanged. */
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Gear|Fixture", meta = (ClampMin = "100"))
	float WaveEnemyHealth = 500.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Gear|Fixture", meta = (ClampMin = "100"))
	float EliteEnemyHealth = 1200.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Knight|Gear|Fixture", meta = (ClampMin = "100"))
	float BossEnemyHealth = 5000.0f;
	TMap<FGuid, FName> FixtureRequests;
	TMap<FGuid, FKnightResult> FixtureResults;
};
