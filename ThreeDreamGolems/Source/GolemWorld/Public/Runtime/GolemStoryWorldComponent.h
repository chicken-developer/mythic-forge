#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Definitions/GolemStoryWorldDefinition.h"
#include "Events/GolemStoryEvent.h"
#include "GolemStoryWorldComponent.generated.h"

class AGolemStoryAnchor;
class APawn;

USTRUCT()
struct GOLEMWORLD_API FGolemWorldFact
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName EventName;
	UPROPERTY(SaveGame)
	FName SubjectId;
	UPROPERTY(SaveGame)
	FName ValueId;
	UPROPERTY(SaveGame)
	bool bPersistent = true;
};

UENUM()
enum class EGolemStoryEncounterPhase : uint8
{
	Idle,
	Active,
	Failed,
	Complete
};

USTRUCT()
struct GOLEMWORLD_API FGolemStoryWorldSnapshot
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName CampaignId;
	UPROPERTY(SaveGame)
	FGuid RunId;
	UPROPERTY(SaveGame)
	int64 Revision = 1;
	UPROPERTY(SaveGame)
	int64 FactRevision = 0;
	UPROPERTY(SaveGame)
	TArray<FGolemWorldFact> Facts;
	UPROPERTY(SaveGame)
	TArray<FGuid> Confirmations;
	UPROPERTY(SaveGame)
	FName CheckpointAnchorId;
	UPROPERTY(SaveGame)
	FGuid EncounterRunId;
	UPROPERTY(SaveGame)
	FName EncounterId;
	UPROPERTY(SaveGame)
	EGolemStoryEncounterPhase EncounterPhase = EGolemStoryEncounterPhase::Idle;
	UPROPERTY(SaveGame)
	FName SceneId;
	UPROPERTY(SaveGame)
	FGuid SceneRunId;
	UPROPERTY(SaveGame)
	FName SceneOutcome;
	UPROPERTY(SaveGame)
	bool bSceneActive = false;
};

DECLARE_MULTICAST_DELEGATE(FOnGolemStoryWorldChanged);

/** Durable state and transient reservations have distinct lifetimes and reset policies. */
UCLASS()
class GOLEMWORLD_API UGolemStoryWorldComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGolemStoryWorldComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGolemResult InitializeWorld(FName CampaignId, const FGuid& RunId);
	FGolemResult BindAnchor(AGolemStoryAnchor* Anchor);
	AGolemStoryAnchor* FindAnchor(FName AnchorId) const;
	FGolemResult ReserveInteraction(FName AnchorId, APawn* Pawn, const FGuid& RequestId,
		int64 ExpectedRevision, float Range, FGuid& OutToken);
	FGolemResult ValidateReservation(const FGuid& Token) const;
	void ReleaseReservation(const FGuid& Token);
	void ReleasePawnReservations(const APawn* Pawn);
	FGolemResult CommitFact(FName EventName, FName SubjectId, FName ValueId = NAME_None,
		bool bPersistent = true, const FGuid& ConfirmationId = FGuid(), int32 Quantity = 1);
	FGolemResult BeginEncounter();
	FGolemResult BeginEncounter(FName EncounterId);
	FGolemResult FinishEncounter(bool bSuccess, const FGuid& ExpectedRunId);
	FGolemResult BeginScene(FName SceneId);
	FGolemResult FinishScene(FName Outcome, const FGuid& ExpectedSceneRun);
	FGolemResult SetCheckpoint(FName AnchorId);
	FGolemResult RestoreSnapshot(const FGolemStoryWorldSnapshot& Snapshot);
	FGolemResult ValidateSnapshot(const FGolemStoryWorldSnapshot& Snapshot) const;
	const FGolemStoryWorldSnapshot& GetSnapshot() const;
	bool HasReservations() const;
	FGolemResult ConfigureMap(UGolemStoryWorldDefinition* Definition, FName RegionId);
	void RevealNearbyMarkers(TConstArrayView<FVector> Positions);
	bool IsMarkerDiscovered(FName MarkerId) const;
	bool IsRegionDiscovered(FName RegionId) const;
	FName GetActiveRegionId() const;
	FGolemResult RequestMapPing(APawn* Pawn, const FGolemEntityId& GolemId, FName MarkerId);
	const TArray<FGolemMapPing>& GetMapPings() const;
	FOnGolemStoryWorldChanged OnSnapshotChanged;

protected:
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	struct FReservation
	{
		FName AnchorId;
		TWeakObjectPtr<APawn> Pawn;
		TWeakObjectPtr<AController> Controller;
		FGuid RequestId;
		int64 Revision = 0;
		double ExpiresAt = 0.0;
		float Range = 0.0f;
	};
	void PruneReservations();
	void PruneMapPings();
	void PublishSnapshot();
	UFUNCTION()
	void OnRep_Snapshot();
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FGolemStoryWorldSnapshot Snapshot;
	UPROPERTY(Transient)
	TObjectPtr<UGolemStoryWorldDefinition> MapDefinition;
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FName ActiveRegionId;
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	TArray<FGolemMapPing> MapPings;
	TMap<FGuid, TWeakObjectPtr<AActor>> PingTargets;
	TMap<FGolemEntityId, double> LastPingTimes;
	TMap<FName, TWeakObjectPtr<AGolemStoryAnchor>> Anchors;
	TMap<FGuid, FReservation> Reservations;
	FTimerHandle LeaseTimer;
	FTimerHandle PingTimer;
};
