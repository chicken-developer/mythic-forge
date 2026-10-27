#pragma once

#include "CoreMinimal.h"
#include "Checkpoint/KnightCheckpointSave.h"
#include "Events/KnightEventSubsystem.h"
#include "RPGLoop/KnightRPGLoopGameMode.h"
#include "KnightStoryGameMode.generated.h"

class AKnightStoryState;
class AKnightProtectedCaravan;
class UKnightAbilitySystemComponent;
class UKnightStoryProfile;
class UKnightCampaignDefinition;
class UKnightStoryWorldDefinition;
struct FKnightItemEvent;
struct FKnightSkillLabEvent;
enum class EKnightLifeState : uint8;

UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightStoryGameMode : public AKnightRPGLoopGameMode
{
	GENERATED_BODY()

public:
	AKnightStoryGameMode();
	virtual void InitGameState() override;
	virtual FKnightResult TryInteract(APlayerController* Player) override;
	virtual FKnightResult TryRestartDrill(APlayerController* Player) override;
	virtual void Logout(AController* Exiting) override;
	virtual bool IsInUpgradeSafeZone(const AActor* Knight) const override;
	FKnightResult ExecuteStoryAction(APlayerController* Player, FName Action, FName AnchorId,
		FName ChoiceId, const FKnightEntityId& ExpectedKnight, const FGuid& RequestId, int64 ExpectedRevision);
	FKnightResult CaptureCheckpoint(FKnightCampaignCheckpoint& OutCheckpoint) const;
	AKnightStoryState* GetStoryState() const;
	const FKnightResult& GetStoryInitializationResult() const;
	FKnightResult TryMapTravel(APlayerController* Player, FName MarkerId,
		const FKnightEntityId& ExpectedKnight, int64 ExpectedRevision);

protected:
	virtual FKnightEntityId CreateKnightIdentity(int32 Slot, const FPrimaryAssetId& HeroId) const override;
	virtual FKnightResult InitializeRPGDomains(UKnightInventoryComponent* Inventory, UKnightRPGComponent* RPG,
		TConstArrayView<FKnightRPGKnightRegistration> Registrations) override;
	virtual FKnightResult ConfigureRPGDomains(UKnightInventoryComponent* Inventory, UKnightRPGComponent* RPG) override;
	virtual bool ShouldIssueLegacyEncounterRewards() const override;
	virtual bool ShouldAdvanceWaveToBoss() const override;
	virtual FKnightResult BuildCaller(APlayerController* Player, FKnightRPGCallerContext& OutCaller) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	FKnightResult ResolveProfile();
	FKnightResult InteractAnchor(APlayerController* Player, FName AnchorId, const FGuid& RequestId);
	FKnightResult InteractAnchor(APlayerController* Player, FName AnchorId, const FGuid& RequestId, FName ChoiceId);
	FKnightResult ClaimQuestReward(APlayerController* Player, FName QuestId);
	FKnightResult SaveStoryCheckpoint();
	FKnightResult TravelStory(APlayerController* Player, const FString& MapPackage);
	void HandleArenaChanged(const FKnightPartyArenaSnapshot& Snapshot);
	void HandleItemFact(const FKnightItemEvent& Event);
	void HandleSkillFact(const FKnightSkillLabEvent& Event);
	void HandleQuestChanged();
	void HandleWorldChanged();
	void HandleCaravanLifeChanged(EKnightLifeState Previous, EKnightLifeState Current);

	UPROPERTY(EditDefaultsOnly, Category = "Knight|Story")
	FPrimaryAssetId ProfileId = FPrimaryAssetId(TEXT("KnightStoryProfile"), TEXT("Demo_M6_V2"));
	UPROPERTY(Transient)
	TObjectPtr<UKnightStoryProfile> Profile;
	UPROPERTY(Transient)
	TObjectPtr<UKnightCampaignDefinition> Campaign;
	UPROPERTY(Transient)
	TObjectPtr<UKnightStoryWorldDefinition> WorldDefinition;
	UPROPERTY(Transient)
	TObjectPtr<AKnightStoryState> Story;
	UPROPERTY(Transient)
	TObjectPtr<AKnightProtectedCaravan> Caravan;
	TArray<TWeakObjectPtr<UKnightAbilitySystemComponent>> SceneBlockedASCs;
	FDelegateHandle WorldHandle;
	FDelegateHandle CaravanHandle;
	FGuid RunId;
	FKnightResult InitializationResult;
	FDelegateHandle ArenaHandle;
	FDelegateHandle QuestHandle;
	FKnightEventSubscription ItemSubscription;
	FKnightEventSubscription SkillSubscription;
	bool bTravelling = false;
	bool bStoryMutation = false;
};
