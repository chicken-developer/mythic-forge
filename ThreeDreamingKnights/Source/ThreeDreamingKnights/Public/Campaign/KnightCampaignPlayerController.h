#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Models/KnightCampaignPanelTypes.h"
#include "Story/KnightStoryPlayerController.h"
#include "KnightCampaignPlayerController.generated.h"

class UKnightCampaignDefinition;
class UKnightStoryWorldDefinition;
class UKnightGameUserSettings;
class UInputMappingContext;
class UInputAction;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UKnightCombatComponent;
class UCameraShakeBase;
class USoundWaveProcedural;
class AKnightStoryAnchor;
struct FKnightCombatSnapshot;
enum class EKnightRPGPanelTab : uint8;

struct FKnightCampaignCooldownView
{
	FText Label;
	double EndTime = 0.0;
};

struct FKnightCampaignThreatView
{
	FText Text;
	double EndTime = 0.0;
};

struct FKnightCampaignHudView
{
	TArray<FText> PartyLines;
	TArray<FKnightCampaignCooldownView> Cooldowns;
	FText Tutorial;
	FText Interaction;
	FText Subtitle;
	FText Companion;
	FText Target;
	FText CombatFeedback;
	double CombatFeedbackEnd = 0.0;
	TArray<FKnightCampaignThreatView> Threats;
	TArray<FVector2D> MinimapMarkers;
	FVector2D PlayerUV = FVector2D::ZeroVector;
	bool bPlayerVisible = false;
	bool bSubtitleBackground = true;
	float SubtitleScale = 1.0f;
	float TextScale = 1.0f;
};

/** Adapts existing domain snapshots into the existing CommonUI panel and a cached HUD model. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightCampaignPlayerController : public AKnightStoryPlayerController
{
	GENERATED_BODY()

public:
	void OpenCampaignPage(EKnightRPGPanelTab Page);
	void RequestCampaignAction(FName Action, FName Context = NAME_None);
	const FKnightCampaignPanelSnapshot& GetCampaignPresentation() const;
	const FKnightCampaignHudView& GetCampaignHud() const;
	const FKnightResult& GetLastCampaignResult() const;
	void RequestCampaignSkill(int32 Slot);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void SetupInputComponent() override;

private:
	void BindCampaignSources();
	void BindCampaignPanel();
	void RefreshCampaignPresentation();
	void ApplyLocalPreferences();
	void HandleKeyRemap(FName ActionId, FKey Key);
	void HandlePanelClosed();
	void HandleControlledCampaignKnight(AKnightCharacter* Previous, AKnightCharacter* Current);
	UFUNCTION()
	void HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);
	void UpdateMapPosition();
	void ReadCheckpointSummary(bool bJustSaved = false);
	FString CurrentSaveStamp() const;
	void RefreshHud();
	void BindHudSources();
	void UnbindHudSources();
	void HandleCombatHud(AKnightCharacter* Source, const FKnightCombatSnapshot& Snapshot);
	void BindThreatSources();
	void ObserveThreat(AActor* Actor);
	void RemoveThreatSources();
	void PlayConfirmedImpactTone();
	void UpdateInteractionHint();
	FText BuildTutorial();
	void OpenPauseMenu();
	void OpenMap();
	void OpenJournal();
	void InstallCampaignSkillInput();
	void RefreshCampaignSkillMappings();
	void RemoveCampaignSkillInput();
	UFUNCTION(Server, Reliable)
	void ServerRequestCampaignSkill(int32 Slot, FKnightEntityId ExpectedKnight);
	UFUNCTION(Client, Reliable)
	void ClientReceiveCampaignSkillResult(FKnightResult Result);
	UFUNCTION(Server, Reliable)
	void ServerExecuteCampaignAction(FName Action, FName Context, FKnightEntityId ExpectedKnight,
		FGuid RequestId, int64 ExpectedRevision);
	UFUNCTION(Client, Reliable)
	void ClientReceiveCampaignResult(FGuid RequestId, FName Action, FKnightResult Result);

	UPROPERTY(Transient)
	TObjectPtr<UKnightStoryWorldDefinition> CampaignWorld;
	UPROPERTY(Transient)
	TObjectPtr<UKnightCampaignDefinition> CampaignDefinition;
	TWeakObjectPtr<AKnightCharacter> MovementKnight;
	FKnightCampaignPanelSnapshot Presentation;
	FKnightCampaignHudView Hud;
	FString SavedStamp;
	FText CheckpointDescription;
	FKnightResult LastCampaignResult;
	FGuid CampaignRequest;
	FName SelectedRegion;
	FName PresentedActiveRegion;
	FName ContinueRegion;
	FName Confirmation;
	FName PendingAction;
	FTimerHandle CampaignReadinessTimer;
	FTimerHandle MenuTimer;
	FDelegateHandle CampaignQuestHandle;
	FDelegateHandle CampaignWorldHandle;
	FDelegateHandle CampaignDialogueHandle;
	FDelegateHandle CampaignPresentationHandle;
	FDelegateHandle CampaignControlHandle;
	FDelegateHandle CampaignRosterHandle;
	FDelegateHandle SettingsHandle;
	FDelegateHandle CampaignPanelHandle;
	FDelegateHandle CampaignRemapHandle;
	FDelegateHandle CampaignClosedHandle;
	FDelegateHandle CompanionTutorialHandle;
	FDelegateHandle TargetHudHandle;
	FDelegateHandle TransportFailureHandle;
	FDelegateHandle InputDeviceHandle;
	FDelegateHandle GameplayFeedbackHandle;
	TArray<TWeakObjectPtr<AKnightStoryAnchor>> CampaignAnchors;
	FDelegateHandle ThreatSpawnHandle;
	struct FThreatBinding
	{
		TWeakObjectPtr<UKnightCombatComponent> Source;
		FDelegateHandle Handle;
	};
	TArray<FThreatBinding> ThreatBindings;
	UPROPERTY(Transient)
	TObjectPtr<USoundWaveProcedural> FeedbackTone;
	TWeakObjectPtr<UKnightCombatComponent> TargetHudSource;
	TWeakObjectPtr<UCameraShakeBase> FeedbackShake;
	TArray<int32> FeedbackCounts;
	FKnightEntityId FeedbackKnight;
	TArray<FSimpleDelegate> HudUnbind;
	TWeakObjectPtr<UInputComponent> CampaignInput;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> CampaignSkillMappings;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CampaignSkillModifier;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> CampaignSkillActions;
	TWeakObjectPtr<UEnhancedInputComponent> CampaignSkillInput;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> CampaignSkillSubsystem;
	TArray<uint32> CampaignSkillBindings;
	int32 CampaignBindingAttempts = 0;
	bool bHasDiskCheckpoint = false;
	bool bFrontEnd = false;
	bool bOwnPause = false;
	bool bOwnFrontInput = false;
	bool bEndingCampaign = false;
	bool bReadCheckpoint = false;
	FVector OnboardingOrigin = FVector::ZeroVector;
};
