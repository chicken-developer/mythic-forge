#pragma once

#include "CoreMinimal.h"
#include "RPGLoop/KnightRPGLoopGameMode.h"
#include "KnightSkillLabGameMode.generated.h"

class AKnightRunestone;
class AKnightFrozenSurface;

/** Cumulative M5 sandbox with isolated unlock seed and transient world adapters. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightSkillLabGameMode : public AKnightRPGLoopGameMode
{
	GENERATED_BODY()

public:
	AKnightSkillLabGameMode();
	virtual FKnightResult TryInteract(APlayerController* Player) override;
	virtual FKnightResult TryRestartDrill(APlayerController* Player) override;
	FGuid GetLabEncounterId() const;

protected:
	virtual FKnightResult ConfigureRPGDomains(UKnightInventoryComponent* Inventory, UKnightRPGComponent* RPG) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleLabArenaChanged(const FKnightPartyArenaSnapshot& Snapshot);
	void ResetLabFixtures();
	void ResetTrainingDummy();
	UPROPERTY(Transient)
	TArray<TObjectPtr<AKnightRunestone>> Runestones;
	UPROPERTY(Transient)
	TObjectPtr<AKnightFrozenSurface> FrozenSurface;
	FGuid EncounterId;
	FDelegateHandle ArenaHandle;
	FTimerHandle DummyReadinessTimer;
};
