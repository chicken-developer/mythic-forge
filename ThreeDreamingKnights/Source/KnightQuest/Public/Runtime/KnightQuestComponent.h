#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Definitions/KnightCampaignDefinition.h"
#include "Events/KnightEventSubsystem.h"
#include "Events/KnightStoryEvent.h"
#include "Types/KnightStoryValues.h"
#include "KnightQuestComponent.generated.h"

UENUM()
enum class EKnightQuestPhase : uint8
{
	Locked,
	Available,
	Active,
	RewardPending,
	Complete
};

USTRUCT()
struct KNIGHTQUEST_API FKnightQuestProgress
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName QuestId;
	UPROPERTY(SaveGame)
	EKnightQuestPhase Phase = EKnightQuestPhase::Locked;
	UPROPERTY(SaveGame)
	FKnightStoryCountMap Counts;
	UPROPERTY(SaveGame)
	int64 ActivatedRevision = 0;
	UPROPERTY(SaveGame)
	FGuid RewardReceipt;
};

USTRUCT()
struct KNIGHTQUEST_API FKnightQuestSnapshot
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FPrimaryAssetId DefinitionId;
	UPROPERTY(SaveGame)
	FName CampaignId;
	UPROPERTY(SaveGame)
	FName ScenarioKind;
	UPROPERTY(SaveGame)
	FGuid RunId;
	UPROPERTY(SaveGame)
	int32 ContentRevision = 1;
	UPROPERTY(SaveGame)
	int64 Revision = 1;
	UPROPERTY(SaveGame)
	int64 FactRevision = 0;
	UPROPERTY(SaveGame)
	TArray<FKnightQuestProgress> Quests;
	UPROPERTY(SaveGame)
	TArray<FGuid> SeenFacts;
	UPROPERTY(SaveGame)
	FKnightStoryChoiceMap Branches;
};

DECLARE_MULTICAST_DELEGATE(FOnKnightQuestChanged);

UCLASS()
class KNIGHTQUEST_API UKnightQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKnightQuestComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FKnightResult InitializeCampaign(UKnightCampaignDefinition* Definition, const FGuid& RunId);
	FKnightResult ValidateSnapshot(const FKnightQuestSnapshot& Snapshot) const;
	FKnightResult RestoreSnapshot(const FKnightQuestSnapshot& Snapshot);
	FKnightResult ActivateQuest(FName QuestId);
	FKnightResult ApplyCurrentFact(const FKnightStoryEvent& Fact);
	FKnightResult CommitReward(FName QuestId, const FGuid& Receipt);
	FKnightResult CommitBranch(FName BranchId, FName ChoiceId, int64 ExpectedRevision);
	FKnightResult SetEncounterRun(const FGuid& EncounterRunId);
	const FKnightQuestSnapshot& GetSnapshot() const;
	const FKnightQuestProgress* FindProgress(FName QuestId) const;
	FOnKnightQuestChanged OnSnapshotChanged;

protected:
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	void HandleFact(const FKnightStoryEvent& Fact);
	bool ApplyFact(const FKnightStoryEvent& Fact, bool bCurrentState);
	void RefreshAvailability(bool bAutoActivate);
	void PublishSnapshot();
	UFUNCTION()
	void OnRep_Snapshot();

	UPROPERTY(Transient)
	TObjectPtr<UKnightCampaignDefinition> Campaign;
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FKnightQuestSnapshot Snapshot;

	FGuid EncounterRun;
	FKnightEventSubscription Subscription;
};
