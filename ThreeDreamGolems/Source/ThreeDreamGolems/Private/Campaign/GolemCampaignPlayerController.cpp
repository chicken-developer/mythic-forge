#include "Campaign/GolemCampaignPlayerController.h"

#include "Assets/GolemAssetManager.h"
#include "Campaign/GolemCampaignEntry.h"
#include "Campaign/GolemCampaignGameInstance.h"
#include "Campaign/GolemEncounterQualityGameMode.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/GolemCharacter.h"
#include "Camera/GolemCameraModeComponent.h"
#include "Checkpoint/GolemCheckpointSubsystem.h"
#include "Definitions/GolemCampaignDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "Input/GolemInputConfig.h"
#include "Inventory/GolemInventoryComponent.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Models/GolemRPGPanelTypes.h"
#include "Party/GolemPartyState.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "Settings/GolemGameUserSettings.h"
#include "Story/GolemStoryGameMode.h"
#include "Story/GolemStoryState.h"
#include "Tags/GolemGameplayTags.h"
#include "TimerManager.h"
#include "Widgets/GolemRPGPanel.h"

void AGolemCampaignPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		GameplayFeedbackHandle = OnCommandResult.AddWeakLambda(this, [this](const FGolemResult& Result)
		{
			if (!Result.IsSuccess())
			{
				Presentation.Feedback = Result.Message.IsEmpty() ? FText::FromName(Result.Context) : Result.Message;
				Hud.CombatFeedback = Presentation.Feedback;
				Hud.CombatFeedbackEnd = GetWorld()->GetGameState() != nullptr
					? GetWorld()->GetGameState()->GetServerWorldTimeSeconds() + 2.5 : GetWorld()->GetTimeSeconds() + 2.5;
				RefreshCampaignPresentation();
			}
		});
		InputDeviceHandle = IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddWeakLambda(this,
			[this](const EInputDeviceConnectionState Connection, const FPlatformUserId User, const FInputDeviceId Device)
		{
			if (GetLocalPlayer() == nullptr || GetLocalPlayer()->GetPlatformUserId() != User
				|| Connection != EInputDeviceConnectionState::Disconnected)
			{
				return;
			}
			ResetControlInput();
			RequestAbilityInput(GolemGameplayTags::Input_Combat_Block, false);
			RequestAbilityInput(GolemGameplayTags::Input_Party_Revive, false);
			Presentation.Feedback = NSLOCTEXT("GolemCampaignUI", "ControllerDisconnected",
				"Controller disconnected. Keyboard and mouse remain available. Reconnect and choose Resume when ready.");
			OpenCampaignPage(EGolemRPGPanelTab::Campaign);
		});
		if (UGolemCampaignGameInstance* Session = Cast<UGolemCampaignGameInstance>(GetGameInstance()))
		{
			Presentation.Feedback = Session->TravelFeedback;
			TransportFailureHandle = Session->OnTransportFailure.AddWeakLambda(this, [this](const FText& Error)
			{
				if (bFrontEnd)
				{
					if (UGolemCampaignGameInstance* CurrentSession = Cast<UGolemCampaignGameInstance>(GetGameInstance()))
					{
						CurrentSession->bEnteredCampaign = false;
					}
				}
				CampaignRequest.Invalidate();
				PendingAction = NAME_None;
				LastCampaignResult = FGolemResult::Failure(EGolemError::LoadFailed, TEXT("CampaignTransport"), Error);
				Presentation.Feedback = Error;
				OpenCampaignPage(EGolemRPGPanelTab::Campaign);
			});
		}
		CampaignControlHandle = OnControlledGolemChanged.AddUObject(this, &ThisClass::HandleControlledCampaignGolem);
		CampaignPresentationHandle = OnRPGPresentationChanged.AddUObject(this, &ThisClass::RefreshCampaignPresentation);
		CompanionTutorialHandle = OnCompanionCommandResult.AddWeakLambda(this, [this](const FGolemResult& Result)
		{
			if (Result.IsSuccess())
			{
				if (UGolemGameUserSettings* Settings = UGolemGameUserSettings::Get())
				{
					Settings->ConfirmTutorial(TEXT("Companion"));
				}
			}
			RefreshCampaignPresentation();
		});
		if (UGolemGameUserSettings* Settings = UGolemGameUserSettings::Get())
		{
			SettingsHandle = Settings->OnPreferencesChanged.AddUObject(this, &ThisClass::ApplyLocalPreferences);
		}
		BindCampaignSources();
		BindThreatSources();
	}
}

void AGolemCampaignPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InstallCampaignSkillInput();
	if (InputComponent == nullptr || CampaignInput.Get() == InputComponent)
	{
		return;
	}
	CampaignInput = InputComponent;
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::OpenPauseMenu).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &ThisClass::OpenMap).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::J, IE_Pressed, this, &ThisClass::OpenJournal).bExecuteWhenPaused = true;
}

void AGolemCampaignPlayerController::BindCampaignSources()
{
	AGolemStoryState* State = GetWorld()->GetGameState<AGolemStoryState>();
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	UGolemDefinition* WorldResolved = nullptr;
	UGolemDefinition* QuestResolved = nullptr;
	if (State == nullptr || State->GetPartyState() == nullptr || !State->GetPartyState()->IsCanonicalRosterReady()
		|| GetControlledGolem() == nullptr || Manager == nullptr
		|| !Manager->ResolveDefinition(State->GetWorldDefinitionId(), UGolemStoryWorldDefinition::StaticClass(), WorldResolved).IsSuccess()
		|| !Manager->ResolveDefinition(State->Quest->GetSnapshot().DefinitionId,
			UGolemCampaignDefinition::StaticClass(), QuestResolved).IsSuccess())
	{
		if (++CampaignBindingAttempts < 100)
		{
			GetWorldTimerManager().SetTimer(CampaignReadinessTimer, this, &ThisClass::BindCampaignSources, 0.1f, false);
		}
		return;
	}
	CampaignWorld = Cast<UGolemStoryWorldDefinition>(WorldResolved);
	CampaignDefinition = Cast<UGolemCampaignDefinition>(QuestResolved);
	CampaignQuestHandle = State->Quest->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshCampaignPresentation);
	CampaignWorldHandle = State->WorldState->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshCampaignPresentation);
	CampaignDialogueHandle = State->Dialogue->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshCampaignPresentation);
	CampaignRosterHandle = State->GetPartyState()->OnRosterChanged.AddUObject(this, &ThisClass::BindHudSources);
	UGolemCampaignGameInstance* Session = Cast<UGolemCampaignGameInstance>(GetGameInstance());
	for (TActorIterator<AGolemCampaignEntry> It(GetWorld()); It; ++It)
	{
		bFrontEnd = GetNetMode() != NM_Client && It->bFrontEndEntry && Session != nullptr && !Session->bEnteredCampaign;
		break;
	}
	if (bFrontEnd)
	{
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		bOwnFrontInput = true;
	}
	else if (Session != nullptr)
	{
		Session->bEnteredCampaign = true;
	}
	HandleControlledCampaignGolem(nullptr, GetControlledGolem());
	BindHudSources();
	ApplyLocalPreferences();
	ReadCheckpointSummary();
	RefreshCampaignPresentation();
	if (bFrontEnd)
	{
		OpenCampaignPage(EGolemRPGPanelTab::Campaign);
	}
}

void AGolemCampaignPlayerController::OpenCampaignPage(const EGolemRPGPanelTab Page)
{
	if (bEndingCampaign)
	{
		return;
	}
	if (GetRPGPanel() == nullptr || !GetRPGPanel()->IsActivated())
	{
		ToggleRPGPanel();
	}
	UGolemRPGPanel* Panel = GetRPGPanel();
	if (Panel == nullptr || !Panel->IsActivated())
	{
		return;
	}
	BindCampaignPanel();
	if (GetNetMode() == NM_Standalone && !bOwnPause)
	{
		bOwnPause = SetPause(true);
	}
	Panel->SetActiveTab(Page);
	RefreshCampaignPresentation();
}

void AGolemCampaignPlayerController::BindCampaignPanel()
{
	if (UGolemRPGPanel* Panel = GetRPGPanel(); Panel != nullptr && !CampaignPanelHandle.IsValid())
	{
		CampaignPanelHandle = Panel->OnCampaignActionRequested.AddUObject(this, &ThisClass::RequestCampaignAction);
		CampaignRemapHandle = Panel->OnCampaignKeyRemapRequested.AddUObject(this, &ThisClass::HandleKeyRemap);
		CampaignClosedHandle = Panel->OnPanelClosed.AddUObject(this, &ThisClass::HandlePanelClosed);
	}
}

void AGolemCampaignPlayerController::OpenPauseMenu()
{
	OpenCampaignPage(EGolemRPGPanelTab::Campaign);
}

void AGolemCampaignPlayerController::OpenMap()
{
	if (!bFrontEnd)
	{
		OpenCampaignPage(EGolemRPGPanelTab::Map);
	}
}

void AGolemCampaignPlayerController::OpenJournal()
{
	if (!bFrontEnd)
	{
		OpenCampaignPage(EGolemRPGPanelTab::Journal);
	}
}

void AGolemCampaignPlayerController::HandlePanelClosed()
{
	if (bOwnPause)
	{
		SetPause(false);
		bOwnPause = false;
	}
	if (bFrontEnd && !bEndingCampaign && !CampaignRequest.IsValid())
	{
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			OpenCampaignPage(EGolemRPGPanelTab::Campaign);
		}));
	}
}

void AGolemCampaignPlayerController::RequestCampaignAction(const FName Action, const FName Context)
{
	if (bEndingCampaign || CampaignRequest.IsValid())
	{
		return;
	}
	if (Action == TEXT("Region"))
	{
		SelectedRegion = Context;
		RefreshCampaignPresentation();
		return;
	}
	if (Action == TEXT("Settings") || Action == TEXT("SettingsBack"))
	{
		OpenCampaignPage(Action == TEXT("Settings") ? EGolemRPGPanelTab::Settings : EGolemRPGPanelTab::Campaign);
		return;
	}
	if (Action == TEXT("Resume"))
	{
		if (!bFrontEnd && GetRPGPanel() != nullptr && GetRPGPanel()->IsActivated())
		{
			ToggleRPGPanel();
		}
		return;
	}
	if (Action == TEXT("CancelConfirm"))
	{
		Confirmation = NAME_None;
		RefreshCampaignPresentation();
		return;
	}
	if (Action == TEXT("InspectLoad"))
	{
		ReadCheckpointSummary();
		Presentation.Feedback = CheckpointDescription;
		RefreshCampaignPresentation();
		return;
	}
	UGolemGameUserSettings* Settings = UGolemGameUserSettings::Get();
	if ((Action == TEXT("SettingUp") || Action == TEXT("SettingDown")) && Settings != nullptr)
	{
		LastCampaignResult = Settings->AdjustPreference(Context, Action == TEXT("SettingUp"));
		Presentation.Feedback = LastCampaignResult.IsSuccess()
			? NSLOCTEXT("GolemCampaignUI", "PreferencesSaved", "Settings applied and saved locally.") : LastCampaignResult.Message;
		RefreshCampaignPresentation();
		return;
	}
	if (Action == TEXT("New") || Action == TEXT("Quit") || Action == TEXT("ResetSettings")
		|| (Action == TEXT("Continue") && !bFrontEnd && SavedStamp != CurrentSaveStamp()))
	{
		Confirmation = Action;
		OpenCampaignPage(EGolemRPGPanelTab::Campaign);
		return;
	}
	FName Operation = Action;
	if (Action == TEXT("Confirm"))
	{
		if (Confirmation.IsNone() || Context != Confirmation)
		{
			return;
		}
		Operation = Confirmation;
		Confirmation = NAME_None;
		if (Operation == TEXT("ResetSettings") && Settings != nullptr)
		{
			Settings->ResetPreferences();
			LastCampaignResult = Settings->GetLastSaveResult();
			Presentation.Feedback = LastCampaignResult.IsSuccess()
				? NSLOCTEXT("GolemCampaignUI", "DefaultsSaved", "Default controls and accessibility preferences saved.")
				: LastCampaignResult.Message;
			OpenCampaignPage(EGolemRPGPanelTab::Settings);
			return;
		}
		if (Operation == TEXT("Quit"))
		{
			UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
			return;
		}
	}
	AGolemStoryState* State = GetWorld()->GetGameState<AGolemStoryState>();
	if (State == nullptr || GetControlledGolem() == nullptr)
	{
		return;
	}
	if ((Operation == TEXT("New") || Operation == TEXT("Continue")) && GetNetMode() != NM_Client)
	{
		if (UGolemCampaignGameInstance* Session = Cast<UGolemCampaignGameInstance>(GetGameInstance()))
		{
			Session->bEnteredCampaign = true;
			Session->TravelFeedback = FText();
		}
	}
	CampaignRequest = FGuid::NewGuid();
	PendingAction = Operation;
	Presentation.Feedback = NSLOCTEXT("GolemCampaignUI", "Pending", "Pending — waiting for authoritative confirmation...");
	RefreshCampaignPresentation();
	const FGuid RequestId = CampaignRequest;
	const FGolemEntityId GolemId = GetControlledGolem()->GetGolemEntityId();
	const int64 Revision = State->WorldState->GetSnapshot().Revision;
	const FName Argument = Operation == TEXT("Continue") && (Context.IsNone() || Action == TEXT("Confirm"))
		? ContinueRegion : Context;
	if (bOwnPause)
	{
		SetPause(false);
		bOwnPause = false;
	}
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
		[this, Operation, Argument, GolemId, RequestId, Revision]()
	{
		ServerExecuteCampaignAction(Operation, Argument, GolemId, RequestId, Revision);
	}));
}

void AGolemCampaignPlayerController::ServerExecuteCampaignAction_Implementation(const FName Action,
	const FName Context, const FGolemEntityId ExpectedGolem, const FGuid RequestId, const int64 ExpectedRevision)
{
	AGolemStoryGameMode* Mode = GetWorld()->GetAuthGameMode<AGolemStoryGameMode>();
	AGolemStoryState* State = GetWorld()->GetGameState<AGolemStoryState>();
	AGolemCharacter* Golem = GetControlledGolem();
	FGolemResult Result = FGolemResult::Failure(EGolemError::NotOwned, TEXT("CampaignCaller"), FText::GetEmpty());
	if (Mode != nullptr && State != nullptr && Golem != nullptr && Golem->GetGolemEntityId() == ExpectedGolem
		&& Golem->GetController() == this && RequestId.IsValid()
		&& State->GetPartyState()->FindGolem(Golem) != nullptr)
	{
		if (Action == TEXT("Difficulty"))
		{
			if (AGolemEncounterQualityGameMode* Quality = Cast<AGolemEncounterQualityGameMode>(Mode))
			{
				Result = Quality->TrySetDifficulty(this, Context);
			}
		}
		else if (Action == TEXT("Ping"))
		{
			Result = State->WorldState->RequestMapPing(Golem, ExpectedGolem, Context);
		}
		else if (Action == TEXT("FastTravel"))
		{
			Result = Mode->TryMapTravel(this, Context, ExpectedGolem, ExpectedRevision);
		}
		else
		{
			Result = Mode->ExecuteStoryAction(this, Action, Context,
				Action == TEXT("Continue") ? Context : NAME_None, ExpectedGolem, RequestId, ExpectedRevision);
		}
	}
	ClientReceiveCampaignResult(RequestId, Action, Result);
}

void AGolemCampaignPlayerController::ClientReceiveCampaignResult_Implementation(const FGuid RequestId,
	const FName Action, const FGolemResult Result)
{
	if (CampaignRequest != RequestId)
	{
		return;
	}
	CampaignRequest.Invalidate();
	PendingAction = NAME_None;
	LastCampaignResult = Result;
	Presentation.Feedback = Result.IsSuccess()
		? (Action == TEXT("Save") ? NSLOCTEXT("GolemCampaignUI", "Saved", "Saved — durable checkpoint verified.")
			: NSLOCTEXT("GolemCampaignUI", "Confirmed", "Confirmed."))
		: (!Result.Message.IsEmpty() ? Result.Message : FText::FromName(Result.Context));
	if (Result.IsSuccess() && Action == TEXT("Save"))
	{
		ReadCheckpointSummary(true);
		if (UGolemGameUserSettings* Settings = UGolemGameUserSettings::Get())
		{
			Settings->ConfirmTutorial(TEXT("Save"));
		}
	}
	if (!Result.IsSuccess() && bFrontEnd)
	{
		if (UGolemCampaignGameInstance* Session = Cast<UGolemCampaignGameInstance>(GetGameInstance()))
		{
			Session->bEnteredCampaign = false;
		}
	}
	if (GetNetMode() == NM_Standalone && GetRPGPanel() != nullptr && GetRPGPanel()->IsActivated()
		&& Action != TEXT("Continue") && Action != TEXT("New") && Action != TEXT("FastTravel") && !bOwnPause)
	{
		bOwnPause = SetPause(true);
	}
	RefreshCampaignPresentation();
}

void AGolemCampaignPlayerController::ApplyLocalPreferences()
{
	UGolemGameUserSettings* Settings = UGolemGameUserSettings::Get();
	if (Settings == nullptr || !Settings->ValidatePreferences().IsSuccess())
	{
		return;
	}
	SetLocalInputPreferences(Settings->Sensitivity, Settings->bInvertY, Settings->bToggleBlock, Settings->bHoldLock);
	if (AGolemCharacter* Golem = GetControlledGolem())
	{
		Golem->GetCameraModeComponent()->SetLocalMotionPreferences(Settings->ShakeScale, Settings->bReducedMotion);
		if (Golem->GetCameraModeComponent()->GetLocalFeedbackScale() == 0.0f && FeedbackShake.IsValid()
			&& PlayerCameraManager != nullptr)
		{
			PlayerCameraManager->StopCameraShake(FeedbackShake.Get(), true);
			FeedbackShake.Reset();
		}
	}
	TMap<FKey, FKey> Remapping;
	for (const FGolemUserKeyBinding& Binding : Settings->KeyBindings)
	{
		Remapping.Add(Binding.DefaultKey, Binding.Key);
	}
	if (UGolemInputConfig* Config = GetGolemInputConfig())
	{
		Config->ApplyDesktopRemapping(Remapping);
		if (ULocalPlayer* Local = GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Input = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Input->RequestRebuildControlMappings();
			}
		}
	}
	Settings->ApplyAudio(this);
	RefreshCampaignSkillMappings();
	RefreshCampaignPresentation();
}

void AGolemCampaignPlayerController::HandleKeyRemap(const FName ActionId, const FKey Key)
{
	if (UGolemGameUserSettings* Settings = UGolemGameUserSettings::Get())
	{
		LastCampaignResult = Settings->RemapKey(ActionId, Key);
		Presentation.Feedback = LastCampaignResult.IsSuccess()
			? NSLOCTEXT("GolemCampaignUI", "Remapped", "Binding saved. Tutorial and interaction glyphs updated.")
			: LastCampaignResult.Message;
		RefreshCampaignPresentation();
	}
}

void AGolemCampaignPlayerController::HandleControlledCampaignGolem(AGolemCharacter* Previous, AGolemCharacter* Current)
{
	if (MovementGolem.IsValid())
	{
		MovementGolem->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::HandleMovementUpdated);
	}
	MovementGolem = Current;
	if (Current != nullptr)
	{
		OnboardingOrigin = Current->GetActorLocation();
		Current->OnCharacterMovementUpdated.AddUniqueDynamic(this, &ThisClass::HandleMovementUpdated);
	}
	RefreshHud();
	UpdateMapPosition();
	if (CampaignWorld != nullptr)
	{
		ApplyLocalPreferences();
	}
}

void AGolemCampaignPlayerController::HandleMovementUpdated(const float DeltaSeconds,
	const FVector OldLocation, const FVector OldVelocity)
{
	UpdateMapPosition();
	if (UGolemGameUserSettings* Settings = UGolemGameUserSettings::Get(); Settings != nullptr
		&& !Settings->HasSeenTutorial(TEXT("Move")) && MovementGolem.IsValid()
		&& FVector::DistSquared(MovementGolem->GetActorLocation(), OnboardingOrigin) >= FMath::Square(200.0))
	{
		Settings->ConfirmTutorial(TEXT("Move"));
		RefreshCampaignPresentation();
	}
}

void AGolemCampaignPlayerController::UpdateMapPosition()
{
	UpdateInteractionHint();
	if (CampaignWorld == nullptr || !MovementGolem.IsValid())
	{
		return;
	}
	AGolemStoryState* State = GetWorld()->GetGameState<AGolemStoryState>();
	const FGolemMapRegion* Region = CampaignWorld->FindRegion(SelectedRegion);
	FVector2D UV = FVector2D::ZeroVector;
	const bool bVisible = State != nullptr && Region != nullptr
		&& SelectedRegion == State->WorldState->GetActiveRegionId()
		&& Region->Project(MovementGolem->GetActorLocation() + FVector(GetWorld()->OriginLocation), UV);
	Presentation.PlayerUV = UV;
	Presentation.bPlayerVisible = bVisible;
	const FGolemMapRegion* ActiveRegion = State != nullptr
		? CampaignWorld->FindRegion(State->WorldState->GetActiveRegionId()) : nullptr;
	Hud.bPlayerVisible = ActiveRegion != nullptr && ActiveRegion->Project(
		MovementGolem->GetActorLocation() + FVector(GetWorld()->OriginLocation), Hud.PlayerUV);
	if (UGolemRPGPanel* Panel = GetRPGPanel())
	{
		Panel->UpdateMapPosition(UV, bVisible);
	}
}

const FGolemCampaignPanelSnapshot& AGolemCampaignPlayerController::GetCampaignPresentation() const
{
	return Presentation;
}

const FGolemCampaignHudView& AGolemCampaignPlayerController::GetCampaignHud() const
{
	return Hud;
}

const FGolemResult& AGolemCampaignPlayerController::GetLastCampaignResult() const
{
	return LastCampaignResult;
}

void AGolemCampaignPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	bEndingCampaign = true;
	RemoveThreatSources();
	IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(InputDeviceHandle);
	OnCommandResult.Remove(GameplayFeedbackHandle);
	if (UGolemCampaignGameInstance* Session = Cast<UGolemCampaignGameInstance>(GetGameInstance()))
	{
		Session->OnTransportFailure.Remove(TransportFailureHandle);
	}
	if (FeedbackShake.IsValid() && PlayerCameraManager != nullptr)
	{
		PlayerCameraManager->StopCameraShake(FeedbackShake.Get(), true);
	}
	RemoveCampaignSkillInput();
	GetWorldTimerManager().ClearTimer(CampaignReadinessTimer);
	GetWorldTimerManager().ClearTimer(MenuTimer);
	if (MovementGolem.IsValid())
	{
		MovementGolem->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::HandleMovementUpdated);
	}
	if (AGolemStoryState* State = GetWorld()->GetGameState<AGolemStoryState>())
	{
		State->Quest->OnSnapshotChanged.Remove(CampaignQuestHandle);
		State->WorldState->OnSnapshotChanged.Remove(CampaignWorldHandle);
		State->Dialogue->OnSnapshotChanged.Remove(CampaignDialogueHandle);
		if (State->GetPartyState() != nullptr)
		{
			State->GetPartyState()->OnRosterChanged.Remove(CampaignRosterHandle);
		}
	}
	OnRPGPresentationChanged.Remove(CampaignPresentationHandle);
	OnControlledGolemChanged.Remove(CampaignControlHandle);
	OnCompanionCommandResult.Remove(CompanionTutorialHandle);
	if (UGolemGameUserSettings* Settings = UGolemGameUserSettings::Get())
	{
		Settings->OnPreferencesChanged.Remove(SettingsHandle);
	}
	if (UGolemRPGPanel* Panel = GetRPGPanel())
	{
		Panel->OnCampaignActionRequested.Remove(CampaignPanelHandle);
		Panel->OnCampaignKeyRemapRequested.Remove(CampaignRemapHandle);
		Panel->OnPanelClosed.Remove(CampaignClosedHandle);
	}
	UnbindHudSources();
	if (bOwnPause)
	{
		SetPause(false);
	}
	if (bOwnFrontInput)
	{
		SetIgnoreMoveInput(false);
		SetIgnoreLookInput(false);
	}
	Super::EndPlay(Reason);
}
