#pragma once

#include "CoreMinimal.h"
#include "Commands/KnightRPGCommand.h"
#include "Models/KnightRPGModels.h"
#include "PartyArena/KnightPartyArenaGameMode.h"
#include "KnightRPGLoopGameMode.generated.h"

class AKnightItemPickup;
class AKnightRPGPlayerController;
class AKnightRPGStateActor;
class AKnightRPGUpgradeStation;
class UKnightLootTableDefinition;
class UKnightInventoryComponent;
class UKnightRPGComponent;

/** The primary module validates human/canonical ownership and composes the host's RPG domains. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightRPGLoopGameMode : public AKnightPartyArenaGameMode
{
	GENERATED_BODY()

public:
	AKnightRPGLoopGameMode();
	virtual void InitGameState() override;
	virtual FKnightResult TryInteract(APlayerController* Player) override;
	virtual FKnightResult TryRestartDrill(APlayerController* Player) override;
	virtual FKnightRPGCommandResult ExecuteRPGCommand(AKnightRPGPlayerController* Player, const FKnightRPGCommand& Command);
	FKnightResult TryContinueRPGLoop(APlayerController* Player);
	AKnightRPGStateActor* GetRPGState() const;
	AKnightRPGUpgradeStation* GetUpgradeStation() const;
	const FKnightResult& GetRPGInitializationResult() const;
	bool IsInUpgradeSafeZone(const AActor* Knight) const;

protected:
	virtual int32 GetInitialInventoryCapacity() const;
	virtual bool ShouldIssueLegacyEncounterRewards() const;
	virtual FKnightResult BuildCaller(APlayerController* Player, FKnightRPGCallerContext& OutCaller) const;
	virtual FKnightResult ConfigureRPGDomains(UKnightInventoryComponent* Inventory, UKnightRPGComponent* RPG);
	virtual FKnightResult InitializeRPGDomains(UKnightInventoryComponent* Inventory, UKnightRPGComponent* RPG,
		TConstArrayView<FKnightRPGKnightRegistration> Registrations);
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	FKnightResult IssueEncounterReward();
	void HandleArenaSnapshot(const FKnightPartyArenaSnapshot& Snapshot);

	UPROPERTY(EditDefaultsOnly, Category = "Knight|RPG|Composition")
	FPrimaryAssetId SkillTreeId = FPrimaryAssetId(TEXT("KnightSkillTree"), TEXT("Foundation"));

	UPROPERTY(EditDefaultsOnly, Category = "Knight|RPG|Composition")
	FPrimaryAssetId RewardTableId = FPrimaryAssetId(TEXT("KnightLootTable"), TEXT("EncounterA"));

	UPROPERTY(EditDefaultsOnly, Category = "Knight|RPG|Composition")
	int32 RewardSeed = 508;

	UPROPERTY(Transient)
	TObjectPtr<AKnightRPGStateActor> RPGState;

	UPROPERTY(Transient)
	TObjectPtr<AKnightRPGUpgradeStation> UpgradeStation;

	UPROPERTY(Transient)
	TObjectPtr<UKnightLootTableDefinition> RewardTable;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AKnightItemPickup>> RewardPickups;

	FGuid RewardId;
	FDelegateHandle ArenaSnapshotHandle;
	FKnightResult RPGInitializationResult;
	bool bIssuingReward = false;
	bool bEndingRPGPlay = false;
};
