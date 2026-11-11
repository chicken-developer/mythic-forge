#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Models/GolemCampaignPanelTypes.h"
#include "Story/GolemStoryPlayerController.h"
#include "GolemCampaignPlayerController.generated.h"

class UGolemCampaignDefinition;
class UGolemStoryWorldDefinition;
class UGolemGameUserSettings;
class UInputMappingContext;
class UInputAction;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UGolemCombatComponent;
class UCameraShakeBase;
class USoundWaveProcedural;
class AGolemStoryAnchor;
struct FGolemCombatSnapshot;
enum class EGolemRPGPanelTab : uint8;

struct FGolemCampaignCooldownView
{
	FText Label;
	double EndTime = 0.0;
};

struct FGolemCampaignThreatView
{
	FText Text;
	double EndTime = 0.0;
};

struct FGolemCampaignHudView
{
	TArray<FText> PartyLines;
	TArray<FGolemCampaignCooldownView> Cooldowns;
	FText Tutorial;
	FText Interaction;
	FText Subtitle;
	FText Companion;
	FText Target;
	FText CombatFeedback;
	double CombatFeedbackEnd = 0.0;
	TArray<FGolemCampaignThreatView> Threats;
	TArray<FVector2D> MinimapMarkers;
	FVector2D PlayerUV = FVector2D::ZeroVector;
	bool bPlayerVisible = false;
	bool bSubtitleBackground = true;
	float SubtitleScale = 1.0f;
	float TextScale = 1.0f;
};

/** Adapts existing domain snapshots into the existing CommonUI panel and a cached HUD model. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemCampaignPlayerController : public AGolemStoryPlayerController
{
	GENERATED_BODY()

public:
	void OpenCampaignPage(EGolemRPGPanelTab Page);
	void RequestCampaignAction(FName Action, FName Context = NAME_None);
	const FGolemCampaignPanelSnapshot& GetCampaignPresentation() const;
	const FGolemCampaignHudView& GetCampaignHud() const;
	const FGolemResult& GetLastCampaignResult() const;
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
	void HandleControlledCampaignGolem(AGolemCharacter* Previous, AGolemCharacter* Current);
	UFUNCTION()
	void HandleMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);
	void UpdateMapPosition();
	void ReadCheckpointSummary(bool bJustSaved = false);
	FString CurrentSaveStamp() const;
	void RefreshHud();
	void BindHudSources();
	void UnbindHudSources();
	void HandleCombatHud(AGolemCharacter* Source, const FGolemCombatSnapshot& Snapshot);
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
	void ServerRequestCampaignSkill(int32 Slot, FGolemEntityId ExpectedGolem);
	UFUNCTION(Client, Reliable)
	void ClientReceiveCampaignSkillResult(FGolemResult Result);
	UFUNCTION(Server, Reliable)
	void ServerExecuteCampaignAction(FName Action, FName Context, FGolemEntityId ExpectedGolem,
		FGuid RequestId, int64 ExpectedRevision);
	UFUNCTION(Client, Reliable)
	void ClientReceiveCampaignResult(FGuid RequestId, FName Action, FGolemResult Result);

	UPROPERTY(Transient)
	TObjectPtr<UGolemStoryWorldDefinition> CampaignWorld;
	UPROPERTY(Transient)
	TObjectPtr<UGolemCampaignDefinition> CampaignDefinition;
	TWeakObjectPtr<AGolemCharacter> MovementGolem;
	FGolemCampaignPanelSnapshot Presentation;
	FGolemCampaignHudView Hud;
	FString SavedStamp;
	FText CheckpointDescription;
	FGolemResult LastCampaignResult;
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
	TArray<TWeakObjectPtr<AGolemStoryAnchor>> CampaignAnchors;
	FDelegateHandle ThreatSpawnHandle;
	struct FThreatBinding
	{
		TWeakObjectPtr<UGolemCombatComponent> Source;
		FDelegateHandle Handle;
	};
	TArray<FThreatBinding> ThreatBindings;
	UPROPERTY(Transient)
	TObjectPtr<USoundWaveProcedural> FeedbackTone;
	TWeakObjectPtr<UGolemCombatComponent> TargetHudSource;
	TWeakObjectPtr<UCameraShakeBase> FeedbackShake;
	TArray<int32> FeedbackCounts;
	FGolemEntityId FeedbackGolem;
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
