#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KnightPartyArenaState.generated.h"

UENUM(BlueprintType)
enum class EKnightArenaStage : uint8
{
	AwaitingStart,
	Wave,
	Boss,
	Victory,
	Defeat
};

UENUM(BlueprintType)
enum class EKnightArenaFailure : uint8
{
	None,
	PartyUnavailable,
	EncounterUnavailable
};

USTRUCT(BlueprintType)
struct THREEDREAMINGKNIGHTS_API FKnightPartyArenaSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Arena")
	EKnightArenaStage Stage = EKnightArenaStage::AwaitingStart;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Arena")
	int32 Revision = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Arena")
	int32 AliveEnemies = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Arena")
	int32 BossPhase = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Arena")
	int32 AliveKnights = 3;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Arena")
	int32 DownedKnights = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Knight|Arena")
	EKnightArenaFailure Failure = EKnightArenaFailure::None;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightPartyArenaChanged, const FKnightPartyArenaSnapshot&);

/** Replicated encounter facts. Only the authoritative composition root commits transitions. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightPartyArenaState : public AActor
{
	GENERATED_BODY()

public:
	AKnightPartyArenaState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	const FKnightPartyArenaSnapshot& GetSnapshot() const;
	FOnKnightPartyArenaChanged OnSnapshotChanged;

private:
	friend class AKnightPartyArenaGameMode;
	void CommitSnapshot(const FKnightPartyArenaSnapshot& Candidate);
	UFUNCTION()
	void OnRep_Snapshot();
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FKnightPartyArenaSnapshot Snapshot;
};
