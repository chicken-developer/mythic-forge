#pragma once

#include "CoreMinimal.h"
#include "Checkpoint/GolemCheckpointSave.h"
#include "Events/GolemEventSubsystem.h"
#include "RPGLoop/GolemRPGLoopGameMode.h"
#include "GolemStoryGameMode.generated.h"

class AGolemStoryState;
class AGolemProtectedCaravan;
class UGolemAbilitySystemComponent;
class UGolemStoryProfile;
class UGolemCampaignDefinition;
class UGolemStoryWorldDefinition;
struct FGolemItemEvent;
struct FGolemSkillLabEvent;
enum class EGolemLifeState : uint8;

UCLASS()
class THREEDREAMGOLEMS_API AGolemStoryGameMode : public AGolemRPGLoopGameMode
{
	GENERATED_BODY()

public:
	AGolemStoryGameMode();
	virtual void InitGameState() override;
	virtual FGolemResult TryInteract(APlayerController* Player) override;
	virtual FGolemResult TryRestartDrill(APlayerController* Player) override;
	virtual void Logout(AController* Exiting) override;
	virtual bool IsInUpgradeSafeZone(const AActor* Golem) const override;
	FGolemResult ExecuteStoryAction(APlayerController* Player, FName Action, FName AnchorId,
		FName ChoiceId, const FGolemEntityId& ExpectedGolem, const FGuid& RequestId, int64 ExpectedRevision);
	FGolemResult CaptureCheckpoint(FGolemCampaignCheckpoint& OutCheckpoint) const;
	AGolemStoryState* GetStoryState() const;
	const FGolemResult& GetStoryInitializationResult() const;
	FGolemResult TryMapTravel(APlayerController* Player, FName MarkerId,
		const FGolemEntityId& ExpectedGolem, int64 ExpectedRevision);

protected:
	virtual FGolemEntityId CreateGolemIdentity(int32 Slot, const FPrimaryAssetId& HeroId) const override;
	virtual FGolemResult InitializeRPGDomains(UGolemInventoryComponent* Inventory, UGolemRPGComponent* RPG,
		TConstArrayView<FGolemRPGGolemRegistration> Registrations) override;
	virtual FGolemResult ConfigureRPGDomains(UGolemInventoryComponent* Inventory, UGolemRPGComponent* RPG) override;
	virtual bool ShouldIssueLegacyEncounterRewards() const override;
	virtual bool ShouldAdvanceWaveToBoss() const override;
	virtual FGolemResult BuildCaller(APlayerController* Player, FGolemRPGCallerContext& OutCaller) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	FGolemResult ResolveProfile();
	FGolemResult InteractAnchor(APlayerController* Player, FName AnchorId, const FGuid& RequestId);
	FGolemResult InteractAnchor(APlayerController* Player, FName AnchorId, const FGuid& RequestId, FName ChoiceId);
	FGolemResult ClaimQuestReward(APlayerController* Player, FName QuestId);
	FGolemResult SaveStoryCheckpoint();
	FGolemResult TravelStory(APlayerController* Player, const FString& MapPackage);
	void HandleArenaChanged(const FGolemPartyArenaSnapshot& Snapshot);
	void HandleItemFact(const FGolemItemEvent& Event);
	void HandleSkillFact(const FGolemSkillLabEvent& Event);
	void HandleQuestChanged();
	void HandleWorldChanged();
	void HandleCaravanLifeChanged(EGolemLifeState Previous, EGolemLifeState Current);

	UPROPERTY(EditDefaultsOnly, Category = "Golem|Story")
	FPrimaryAssetId ProfileId = FPrimaryAssetId(TEXT("GolemStoryProfile"), TEXT("Demo_M6_V2"));
	UPROPERTY(Transient)
	TObjectPtr<UGolemStoryProfile> Profile;
	UPROPERTY(Transient)
	TObjectPtr<UGolemCampaignDefinition> Campaign;
	UPROPERTY(Transient)
	TObjectPtr<UGolemStoryWorldDefinition> WorldDefinition;
	UPROPERTY(Transient)
	TObjectPtr<AGolemStoryState> Story;
	UPROPERTY(Transient)
	TObjectPtr<AGolemProtectedCaravan> Caravan;
	TArray<TWeakObjectPtr<UGolemAbilitySystemComponent>> SceneBlockedASCs;
	FDelegateHandle WorldHandle;
	FDelegateHandle CaravanHandle;
	FGuid RunId;
	FGolemResult InitializationResult;
	FDelegateHandle ArenaHandle;
	FDelegateHandle QuestHandle;
	FGolemEventSubscription ItemSubscription;
	FGolemEventSubscription SkillSubscription;
	bool bTravelling = false;
	bool bStoryMutation = false;
};
