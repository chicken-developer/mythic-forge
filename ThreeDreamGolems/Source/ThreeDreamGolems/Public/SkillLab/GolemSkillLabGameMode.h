#pragma once

#include "CoreMinimal.h"
#include "RPGLoop/GolemRPGLoopGameMode.h"
#include "GolemSkillLabGameMode.generated.h"

class AGolemRunestone;
class AGolemFrozenSurface;

/** Cumulative M5 sandbox with isolated unlock seed and transient world adapters. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemSkillLabGameMode : public AGolemRPGLoopGameMode
{
	GENERATED_BODY()

public:
	AGolemSkillLabGameMode();
	virtual FGolemResult TryInteract(APlayerController* Player) override;
	virtual FGolemResult TryRestartDrill(APlayerController* Player) override;
	FGuid GetLabEncounterId() const;

protected:
	virtual FGolemResult ConfigureRPGDomains(UGolemInventoryComponent* Inventory, UGolemRPGComponent* RPG) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleLabArenaChanged(const FGolemPartyArenaSnapshot& Snapshot);
	void ResetLabFixtures();
	void ResetTrainingDummy();
	UPROPERTY(Transient)
	TArray<TObjectPtr<AGolemRunestone>> Runestones;
	UPROPERTY(Transient)
	TObjectPtr<AGolemFrozenSurface> FrozenSurface;
	FGuid EncounterId;
	FDelegateHandle ArenaHandle;
	FTimerHandle DummyReadinessTimer;
};
