#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Definitions/GolemCampaignDefinition.h"
#include "Events/GolemEventSubsystem.h"
#include "Events/GolemStoryEvent.h"
#include "Types/GolemStoryValues.h"
#include "GolemQuestComponent.generated.h"

UENUM()
enum class EGolemQuestPhase : uint8
{
	Locked,
	Available,
	Active,
	RewardPending,
	Complete
};

USTRUCT()
struct GOLEMQUEST_API FGolemQuestProgress
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName QuestId;
	UPROPERTY(SaveGame)
	EGolemQuestPhase Phase = EGolemQuestPhase::Locked;
	UPROPERTY(SaveGame)
	FGolemStoryCountMap Counts;
	UPROPERTY(SaveGame)
	int64 ActivatedRevision = 0;
	UPROPERTY(SaveGame)
	FGuid RewardReceipt;
};

USTRUCT()
struct GOLEMQUEST_API FGolemQuestSnapshot
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
	TArray<FGolemQuestProgress> Quests;
	UPROPERTY(SaveGame)
	TArray<FGuid> SeenFacts;
	UPROPERTY(SaveGame)
	FGolemStoryChoiceMap Branches;
};

DECLARE_MULTICAST_DELEGATE(FOnGolemQuestChanged);

UCLASS()
class GOLEMQUEST_API UGolemQuestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGolemQuestComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGolemResult InitializeCampaign(UGolemCampaignDefinition* Definition, const FGuid& RunId);
	FGolemResult ValidateSnapshot(const FGolemQuestSnapshot& Snapshot) const;
	FGolemResult RestoreSnapshot(const FGolemQuestSnapshot& Snapshot);
	FGolemResult ActivateQuest(FName QuestId);
	FGolemResult ApplyCurrentFact(const FGolemStoryEvent& Fact);
	FGolemResult CommitReward(FName QuestId, const FGuid& Receipt);
	FGolemResult CommitBranch(FName BranchId, FName ChoiceId, int64 ExpectedRevision);
	FGolemResult SetEncounterRun(const FGuid& EncounterRunId);
	const FGolemQuestSnapshot& GetSnapshot() const;
	const FGolemQuestProgress* FindProgress(FName QuestId) const;
	FOnGolemQuestChanged OnSnapshotChanged;

protected:
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	void HandleFact(const FGolemStoryEvent& Fact);
	bool ApplyFact(const FGolemStoryEvent& Fact, bool bCurrentState);
	void RefreshAvailability(bool bAutoActivate);
	void PublishSnapshot();
	UFUNCTION()
	void OnRep_Snapshot();

	UPROPERTY(Transient)
	TObjectPtr<UGolemCampaignDefinition> Campaign;
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FGolemQuestSnapshot Snapshot;

	FGuid EncounterRun;
	FGolemEventSubscription Subscription;
};
