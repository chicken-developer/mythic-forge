#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Definitions/KnightStoryWorldDefinition.h"
#include "Events/KnightStoryEvent.h"
#include "KnightStoryWorldComponent.generated.h"

class AKnightStoryAnchor;
class APawn;

USTRUCT()
struct KNIGHTWORLD_API FKnightWorldFact
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
enum class EKnightStoryEncounterPhase : uint8
{
	Idle,
	Active,
	Failed,
	Complete
};

USTRUCT()
struct KNIGHTWORLD_API FKnightStoryWorldSnapshot
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
	TArray<FKnightWorldFact> Facts;
	UPROPERTY(SaveGame)
	TArray<FGuid> Confirmations;
	UPROPERTY(SaveGame)
	FName CheckpointAnchorId;
	UPROPERTY(SaveGame)
	FGuid EncounterRunId;
	UPROPERTY(SaveGame)
	FName EncounterId;
	UPROPERTY(SaveGame)
	EKnightStoryEncounterPhase EncounterPhase = EKnightStoryEncounterPhase::Idle;
	UPROPERTY(SaveGame)
	FName SceneId;
	UPROPERTY(SaveGame)
	FGuid SceneRunId;
	UPROPERTY(SaveGame)
	FName SceneOutcome;
	UPROPERTY(SaveGame)
	bool bSceneActive = false;
};

DECLARE_MULTICAST_DELEGATE(FOnKnightStoryWorldChanged);

/** Durable state and transient reservations have distinct lifetimes and reset policies. */
UCLASS()
class KNIGHTWORLD_API UKnightStoryWorldComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKnightStoryWorldComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FKnightResult InitializeWorld(FName CampaignId, const FGuid& RunId);
	FKnightResult BindAnchor(AKnightStoryAnchor* Anchor);
	AKnightStoryAnchor* FindAnchor(FName AnchorId) const;
	FKnightResult ReserveInteraction(FName AnchorId, APawn* Pawn, const FGuid& RequestId,
		int64 ExpectedRevision, float Range, FGuid& OutToken);
	FKnightResult ValidateReservation(const FGuid& Token) const;
	void ReleaseReservation(const FGuid& Token);
	void ReleasePawnReservations(const APawn* Pawn);
	FKnightResult CommitFact(FName EventName, FName SubjectId, FName ValueId = NAME_None,
		bool bPersistent = true, const FGuid& ConfirmationId = FGuid(), int32 Quantity = 1);
	FKnightResult BeginEncounter();
	FKnightResult BeginEncounter(FName EncounterId);
	FKnightResult FinishEncounter(bool bSuccess, const FGuid& ExpectedRunId);
	FKnightResult BeginScene(FName SceneId);
	FKnightResult FinishScene(FName Outcome, const FGuid& ExpectedSceneRun);
	FKnightResult SetCheckpoint(FName AnchorId);
	FKnightResult RestoreSnapshot(const FKnightStoryWorldSnapshot& Snapshot);
	FKnightResult ValidateSnapshot(const FKnightStoryWorldSnapshot& Snapshot) const;
	const FKnightStoryWorldSnapshot& GetSnapshot() const;
	bool HasReservations() const;
	FKnightResult ConfigureMap(UKnightStoryWorldDefinition* Definition, FName RegionId);
	void RevealNearbyMarkers(TConstArrayView<FVector> Positions);
	bool IsMarkerDiscovered(FName MarkerId) const;
	bool IsRegionDiscovered(FName RegionId) const;
	FName GetActiveRegionId() const;
	FKnightResult RequestMapPing(APawn* Pawn, const FKnightEntityId& KnightId, FName MarkerId);
	const TArray<FKnightMapPing>& GetMapPings() const;
	FOnKnightStoryWorldChanged OnSnapshotChanged;

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
	FKnightStoryWorldSnapshot Snapshot;
	UPROPERTY(Transient)
	TObjectPtr<UKnightStoryWorldDefinition> MapDefinition;
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FName ActiveRegionId;
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	TArray<FKnightMapPing> MapPings;
	TMap<FGuid, TWeakObjectPtr<AActor>> PingTargets;
	TMap<FKnightEntityId, double> LastPingTimes;
	TMap<FName, TWeakObjectPtr<AKnightStoryAnchor>> Anchors;
	TMap<FGuid, FReservation> Reservations;
	FTimerHandle LeaseTimer;
	FTimerHandle PingTimer;
};
