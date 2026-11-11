#pragma once

#include "CoreMinimal.h"
#include "Commands/GolemRPGCommand.h"
#include "Models/GolemRPGModels.h"
#include "PartyArena/GolemPartyArenaGameMode.h"
#include "GolemRPGLoopGameMode.generated.h"

class AGolemItemPickup;
class AGolemRPGPlayerController;
class AGolemRPGStateActor;
class AGolemRPGUpgradeStation;
class UGolemLootTableDefinition;
class UGolemInventoryComponent;
class UGolemRPGComponent;

/** The primary module validates human/canonical ownership and composes the host's RPG domains. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemRPGLoopGameMode : public AGolemPartyArenaGameMode
{
	GENERATED_BODY()

public:
	AGolemRPGLoopGameMode();
	virtual void InitGameState() override;
	virtual FGolemResult TryInteract(APlayerController* Player) override;
	virtual FGolemResult TryRestartDrill(APlayerController* Player) override;
	virtual FGolemRPGCommandResult ExecuteRPGCommand(AGolemRPGPlayerController* Player, const FGolemRPGCommand& Command);
	FGolemResult TryContinueRPGLoop(APlayerController* Player);
	AGolemRPGStateActor* GetRPGState() const;
	AGolemRPGUpgradeStation* GetUpgradeStation() const;
	const FGolemResult& GetRPGInitializationResult() const;
	virtual bool IsInUpgradeSafeZone(const AActor* Golem) const;

protected:
	virtual int32 GetInitialInventoryCapacity() const;
	virtual bool ShouldIssueLegacyEncounterRewards() const;
	virtual FGolemResult BuildCaller(APlayerController* Player, FGolemRPGCallerContext& OutCaller) const;
	virtual FGolemResult ConfigureRPGDomains(UGolemInventoryComponent* Inventory, UGolemRPGComponent* RPG);
	virtual FGolemResult InitializeRPGDomains(UGolemInventoryComponent* Inventory, UGolemRPGComponent* RPG,
		TConstArrayView<FGolemRPGGolemRegistration> Registrations);
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	FGolemResult IssueEncounterReward();
	void HandleArenaSnapshot(const FGolemPartyArenaSnapshot& Snapshot);

	UPROPERTY(EditDefaultsOnly, Category = "Golem|RPG|Composition")
	FPrimaryAssetId SkillTreeId = FPrimaryAssetId(TEXT("KnightSkillTree"), TEXT("Foundation"));

	UPROPERTY(EditDefaultsOnly, Category = "Golem|RPG|Composition")
	FPrimaryAssetId RewardTableId = FPrimaryAssetId(TEXT("KnightLootTable"), TEXT("EncounterA"));

	UPROPERTY(EditDefaultsOnly, Category = "Golem|RPG|Composition")
	int32 RewardSeed = 508;

	UPROPERTY(Transient)
	TObjectPtr<AGolemRPGStateActor> RPGState;

	UPROPERTY(Transient)
	TObjectPtr<AGolemRPGUpgradeStation> UpgradeStation;

	UPROPERTY(Transient)
	TObjectPtr<UGolemLootTableDefinition> RewardTable;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGolemItemPickup>> RewardPickups;

	FGuid RewardId;
	FDelegateHandle ArenaSnapshotHandle;
	FGolemResult RPGInitializationResult;
	bool bIssuingReward = false;
	bool bEndingRPGPlay = false;
};
