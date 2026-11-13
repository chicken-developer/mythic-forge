#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GolemPartyArenaState.generated.h"

UENUM(BlueprintType)
enum class EGolemArenaStage : uint8
{
	AwaitingStart,
	Wave,
	Boss,
	Victory,
	Defeat
};

UENUM(BlueprintType)
enum class EGolemArenaFailure : uint8
{
	None,
	PartyUnavailable,
	EncounterUnavailable
};

USTRUCT(BlueprintType)
struct THREEDREAMGOLEMS_API FGolemPartyArenaSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Arena")
	EGolemArenaStage Stage = EGolemArenaStage::AwaitingStart;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Arena")
	int32 Revision = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Arena")
	int32 AliveEnemies = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Arena")
	int32 BossPhase = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Arena")
	int32 AliveGolems = 3;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Arena")
	int32 DownedGolems = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Golem|Arena")
	EGolemArenaFailure Failure = EGolemArenaFailure::None;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGolemPartyArenaChanged, const FGolemPartyArenaSnapshot&);

/** Replicated encounter facts. Only the authoritative composition root commits transitions. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemPartyArenaState : public AActor
{
	GENERATED_BODY()

public:
	AGolemPartyArenaState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	const FGolemPartyArenaSnapshot& GetSnapshot() const;
	FOnGolemPartyArenaChanged OnSnapshotChanged;

private:
	friend class AGolemPartyArenaGameMode;
	void CommitSnapshot(const FGolemPartyArenaSnapshot& Candidate);
	UFUNCTION()
	void OnRep_Snapshot();
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FGolemPartyArenaSnapshot Snapshot;
};
