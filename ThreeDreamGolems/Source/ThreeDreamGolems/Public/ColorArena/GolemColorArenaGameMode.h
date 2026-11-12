#pragma once

#include "CoreMinimal.h"
#include "PartyArena/GolemPartyArenaGameMode.h"
#include "GolemColorArenaGameMode.generated.h"

class AGolemXenon;

/** One Xenon of the M0 sandbox: which definition, where, and the seed of its void colours. */
USTRUCT()
struct FGolemColorArenaSpawn
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Golem|ColorArena")
	FPrimaryAssetId XenonId;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|ColorArena")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|ColorArena")
	int32 Seed = 0;
};

/**
 * M0 grey-box arena: the three Golems and their companions from the party arena, facing Xenons
 * instead of the old wave and boss. Restart (R) puts fresh Xenons back.
 */
UCLASS()
class THREEDREAMGOLEMS_API AGolemColorArenaGameMode : public AGolemPartyArenaGameMode
{
	GENERATED_BODY()

public:
	AGolemColorArenaGameMode();

	/** The old encounter start is replaced by the Xenons that are already standing in the arena. */
	virtual FGolemResult TryInteract(APlayerController* Player) override;
	virtual FGolemResult TryRestartDrill(APlayerController* Player) override;

	const TArray<TObjectPtr<AGolemXenon>>& GetXenons() const { return Xenons; }
	FGolemResult RespawnXenons();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "Golem|ColorArena")
	TArray<FGolemColorArenaSpawn> XenonSpawns;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<AGolemXenon>> Xenons;
};
