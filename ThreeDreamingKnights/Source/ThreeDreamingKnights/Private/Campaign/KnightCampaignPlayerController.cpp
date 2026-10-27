#include "Campaign/KnightCampaignPlayerController.h"

#include "Assets/KnightAssetManager.h"
#include "Campaign/KnightCampaignEntry.h"
#include "Campaign/KnightCampaignGameInstance.h"
#include "Campaign/KnightEncounterQualityGameMode.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/KnightCharacter.h"
#include "Camera/KnightCameraModeComponent.h"
#include "Checkpoint/KnightCheckpointSubsystem.h"
#include "Definitions/KnightCampaignDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "Input/KnightInputConfig.h"
#include "Inventory/KnightInventoryComponent.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Models/KnightRPGPanelTypes.h"
#include "Party/KnightPartyState.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "Settings/KnightGameUserSettings.h"
#include "Story/KnightStoryGameMode.h"
#include "Story/KnightStoryState.h"
#include "Tags/KnightGameplayTags.h"
#include "TimerManager.h"
#include "Widgets/KnightRPGPanel.h"

void AKnightCampaignPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		GameplayFeedbackHandle = OnCommandResult.AddWeakLambda(this, [this](const FKnightResult& Result)
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
			RequestAbilityInput(KnightGameplayTags::Input_Combat_Block, false);
			RequestAbilityInput(KnightGameplayTags::Input_Party_Revive, false);
			Presentation.Feedback = NSLOCTEXT("KnightCampaignUI", "ControllerDisconnected",
				"Controller disconnected. Keyboard and mouse remain available. Reconnect and choose Resume when ready.");
			OpenCampaignPage(EKnightRPGPanelTab::Campaign);
		});
		if (UKnightCampaignGameInstance* Session = Cast<UKnightCampaignGameInstance>(GetGameInstance()))
		{
			Presentation.Feedback = Session->TravelFeedback;
			TransportFailureHandle = Session->OnTransportFailure.AddWeakLambda(this, [this](const FText& Error)
			{
				if (bFrontEnd)
				{
					if (UKnightCampaignGameInstance* CurrentSession = Cast<UKnightCampaignGameInstance>(GetGameInstance()))
					{
						CurrentSession->bEnteredCampaign = false;
					}
				}
				CampaignRequest.Invalidate();
				PendingAction = NAME_None;
				LastCampaignResult = FKnightResult::Failure(EKnightError::LoadFailed, TEXT("CampaignTransport"), Error);
				Presentation.Feedback = Error;
				OpenCampaignPage(EKnightRPGPanelTab::Campaign);
			});
		}
		CampaignControlHandle = OnControlledKnightChanged.AddUObject(this, &ThisClass::HandleControlledCampaignKnight);
		CampaignPresentationHandle = OnRPGPresentationChanged.AddUObject(this, &ThisClass::RefreshCampaignPresentation);
		CompanionTutorialHandle = OnCompanionCommandResult.AddWeakLambda(this, [this](const FKnightResult& Result)
		{
			if (Result.IsSuccess())
			{
				if (UKnightGameUserSettings* Settings = UKnightGameUserSettings::Get())
				{
					Settings->ConfirmTutorial(TEXT("Companion"));
				}
			}
			RefreshCampaignPresentation();
		});
		if (UKnightGameUserSettings* Settings = UKnightGameUserSettings::Get())
		{
			SettingsHandle = Settings->OnPreferencesChanged.AddUObject(this, &ThisClass::ApplyLocalPreferences);
		}
		BindCampaignSources();
		BindThreatSources();
	}
}

void AKnightCampaignPlayerController::SetupInputComponent()
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

void AKnightCampaignPlayerController::BindCampaignSources()
{
	AKnightStoryState* State = GetWorld()->GetGameState<AKnightStoryState>();
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	UKnightDefinition* WorldResolved = nullptr;
	UKnightDefinition* QuestResolved = nullptr;
	if (State == nullptr || State->GetPartyState() == nullptr || !State->GetPartyState()->IsCanonicalRosterReady()
		|| GetControlledKnight() == nullptr || Manager == nullptr
		|| !Manager->ResolveDefinition(State->GetWorldDefinitionId(), UKnightStoryWorldDefinition::StaticClass(), WorldResolved).IsSuccess()
		|| !Manager->ResolveDefinition(State->Quest->GetSnapshot().DefinitionId,
			UKnightCampaignDefinition::StaticClass(), QuestResolved).IsSuccess())
	{
		if (++CampaignBindingAttempts < 100)
		{
			GetWorldTimerManager().SetTimer(CampaignReadinessTimer, this, &ThisClass::BindCampaignSources, 0.1f, false);
		}
		return;
	}
	CampaignWorld = Cast<UKnightStoryWorldDefinition>(WorldResolved);
	CampaignDefinition = Cast<UKnightCampaignDefinition>(QuestResolved);
	CampaignQuestHandle = State->Quest->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshCampaignPresentation);
	CampaignWorldHandle = State->WorldState->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshCampaignPresentation);
	CampaignDialogueHandle = State->Dialogue->OnSnapshotChanged.AddUObject(this, &ThisClass::RefreshCampaignPresentation);
	CampaignRosterHandle = State->GetPartyState()->OnRosterChanged.AddUObject(this, &ThisClass::BindHudSources);
	UKnightCampaignGameInstance* Session = Cast<UKnightCampaignGameInstance>(GetGameInstance());
	for (TActorIterator<AKnightCampaignEntry> It(GetWorld()); It; ++It)
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
	HandleControlledCampaignKnight(nullptr, GetControlledKnight());
	BindHudSources();
	ApplyLocalPreferences();
	ReadCheckpointSummary();
	RefreshCampaignPresentation();
	if (bFrontEnd)
	{
		OpenCampaignPage(EKnightRPGPanelTab::Campaign);
	}
}

void AKnightCampaignPlayerController::OpenCampaignPage(const EKnightRPGPanelTab Page)
{
	if (bEndingCampaign)
	{
		return;
	}
	if (GetRPGPanel() == nullptr || !GetRPGPanel()->IsActivated())
	{
		ToggleRPGPanel();
	}
	UKnightRPGPanel* Panel = GetRPGPanel();
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

void AKnightCampaignPlayerController::BindCampaignPanel()
{
	if (UKnightRPGPanel* Panel = GetRPGPanel(); Panel != nullptr && !CampaignPanelHandle.IsValid())
	{
		CampaignPanelHandle = Panel->OnCampaignActionRequested.AddUObject(this, &ThisClass::RequestCampaignAction);
		CampaignRemapHandle = Panel->OnCampaignKeyRemapRequested.AddUObject(this, &ThisClass::HandleKeyRemap);
		CampaignClosedHandle = Panel->OnPanelClosed.AddUObject(this, &ThisClass::HandlePanelClosed);
	}
}

void AKnightCampaignPlayerController::OpenPauseMenu()
{
	OpenCampaignPage(EKnightRPGPanelTab::Campaign);
}

void AKnightCampaignPlayerController::OpenMap()
{
	if (!bFrontEnd)
	{
		OpenCampaignPage(EKnightRPGPanelTab::Map);
	}
}

void AKnightCampaignPlayerController::OpenJournal()
{
	if (!bFrontEnd)
	{
		OpenCampaignPage(EKnightRPGPanelTab::Journal);
	}
}

void AKnightCampaignPlayerController::HandlePanelClosed()
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
			OpenCampaignPage(EKnightRPGPanelTab::Campaign);
		}));
	}
}

void AKnightCampaignPlayerController::RequestCampaignAction(const FName Action, const FName Context)
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
		OpenCampaignPage(Action == TEXT("Settings") ? EKnightRPGPanelTab::Settings : EKnightRPGPanelTab::Campaign);
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
	UKnightGameUserSettings* Settings = UKnightGameUserSettings::Get();
	if ((Action == TEXT("SettingUp") || Action == TEXT("SettingDown")) && Settings != nullptr)
	{
		LastCampaignResult = Settings->AdjustPreference(Context, Action == TEXT("SettingUp"));
		Presentation.Feedback = LastCampaignResult.IsSuccess()
			? NSLOCTEXT("KnightCampaignUI", "PreferencesSaved", "Settings applied and saved locally.") : LastCampaignResult.Message;
		RefreshCampaignPresentation();
		return;
	}
	if (Action == TEXT("New") || Action == TEXT("Quit") || Action == TEXT("ResetSettings")
		|| (Action == TEXT("Continue") && !bFrontEnd && SavedStamp != CurrentSaveStamp()))
	{
		Confirmation = Action;
		OpenCampaignPage(EKnightRPGPanelTab::Campaign);
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
				? NSLOCTEXT("KnightCampaignUI", "DefaultsSaved", "Default controls and accessibility preferences saved.")
				: LastCampaignResult.Message;
			OpenCampaignPage(EKnightRPGPanelTab::Settings);
			return;
		}
		if (Operation == TEXT("Quit"))
		{
			UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
			return;
		}
	}
	AKnightStoryState* State = GetWorld()->GetGameState<AKnightStoryState>();
	if (State == nullptr || GetControlledKnight() == nullptr)
	{
		return;
	}
	if ((Operation == TEXT("New") || Operation == TEXT("Continue")) && GetNetMode() != NM_Client)
	{
		if (UKnightCampaignGameInstance* Session = Cast<UKnightCampaignGameInstance>(GetGameInstance()))
		{
			Session->bEnteredCampaign = true;
			Session->TravelFeedback = FText();
		}
	}
	CampaignRequest = FGuid::NewGuid();
	PendingAction = Operation;
	Presentation.Feedback = NSLOCTEXT("KnightCampaignUI", "Pending", "Pending — waiting for authoritative confirmation...");
	RefreshCampaignPresentation();
	const FGuid RequestId = CampaignRequest;
	const FKnightEntityId KnightId = GetControlledKnight()->GetKnightEntityId();
	const int64 Revision = State->WorldState->GetSnapshot().Revision;
	const FName Argument = Operation == TEXT("Continue") && (Context.IsNone() || Action == TEXT("Confirm"))
		? ContinueRegion : Context;
	if (bOwnPause)
	{
		SetPause(false);
		bOwnPause = false;
	}
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
		[this, Operation, Argument, KnightId, RequestId, Revision]()
	{
		ServerExecuteCampaignAction(Operation, Argument, KnightId, RequestId, Revision);
	}));
}

void AKnightCampaignPlayerController::ServerExecuteCampaignAction_Implementation(const FName Action,
	const FName Context, const FKnightEntityId ExpectedKnight, const FGuid RequestId, const int64 ExpectedRevision)
{
	AKnightStoryGameMode* Mode = GetWorld()->GetAuthGameMode<AKnightStoryGameMode>();
	AKnightStoryState* State = GetWorld()->GetGameState<AKnightStoryState>();
	AKnightCharacter* Knight = GetControlledKnight();
	FKnightResult Result = FKnightResult::Failure(EKnightError::NotOwned, TEXT("CampaignCaller"), FText::GetEmpty());
	if (Mode != nullptr && State != nullptr && Knight != nullptr && Knight->GetKnightEntityId() == ExpectedKnight
		&& Knight->GetController() == this && RequestId.IsValid()
		&& State->GetPartyState()->FindKnight(Knight) != nullptr)
	{
		if (Action == TEXT("Difficulty"))
		{
			if (AKnightEncounterQualityGameMode* Quality = Cast<AKnightEncounterQualityGameMode>(Mode))
			{
				Result = Quality->TrySetDifficulty(this, Context);
			}
		}
		else if (Action == TEXT("Ping"))
		{
			Result = State->WorldState->RequestMapPing(Knight, ExpectedKnight, Context);
		}
		else if (Action == TEXT("FastTravel"))
		{
			Result = Mode->TryMapTravel(this, Context, ExpectedKnight, ExpectedRevision);
		}
		else
		{
			Result = Mode->ExecuteStoryAction(this, Action, Context,
				Action == TEXT("Continue") ? Context : NAME_None, ExpectedKnight, RequestId, ExpectedRevision);
		}
	}
	ClientReceiveCampaignResult(RequestId, Action, Result);
}

void AKnightCampaignPlayerController::ClientReceiveCampaignResult_Implementation(const FGuid RequestId,
	const FName Action, const FKnightResult Result)
{
	if (CampaignRequest != RequestId)
	{
		return;
	}
	CampaignRequest.Invalidate();
	PendingAction = NAME_None;
	LastCampaignResult = Result;
	Presentation.Feedback = Result.IsSuccess()
		? (Action == TEXT("Save") ? NSLOCTEXT("KnightCampaignUI", "Saved", "Saved — durable checkpoint verified.")
			: NSLOCTEXT("KnightCampaignUI", "Confirmed", "Confirmed."))
		: (!Result.Message.IsEmpty() ? Result.Message : FText::FromName(Result.Context));
	if (Result.IsSuccess() && Action == TEXT("Save"))
	{
		ReadCheckpointSummary(true);
		if (UKnightGameUserSettings* Settings = UKnightGameUserSettings::Get())
		{
			Settings->ConfirmTutorial(TEXT("Save"));
		}
	}
	if (!Result.IsSuccess() && bFrontEnd)
	{
		if (UKnightCampaignGameInstance* Session = Cast<UKnightCampaignGameInstance>(GetGameInstance()))
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

void AKnightCampaignPlayerController::ApplyLocalPreferences()
{
	UKnightGameUserSettings* Settings = UKnightGameUserSettings::Get();
	if (Settings == nullptr || !Settings->ValidatePreferences().IsSuccess())
	{
		return;
	}
	SetLocalInputPreferences(Settings->Sensitivity, Settings->bInvertY, Settings->bToggleBlock, Settings->bHoldLock);
	if (AKnightCharacter* Knight = GetControlledKnight())
	{
		Knight->GetCameraModeComponent()->SetLocalMotionPreferences(Settings->ShakeScale, Settings->bReducedMotion);
		if (Knight->GetCameraModeComponent()->GetLocalFeedbackScale() == 0.0f && FeedbackShake.IsValid()
			&& PlayerCameraManager != nullptr)
		{
			PlayerCameraManager->StopCameraShake(FeedbackShake.Get(), true);
			FeedbackShake.Reset();
		}
	}
	TMap<FKey, FKey> Remapping;
	for (const FKnightUserKeyBinding& Binding : Settings->KeyBindings)
	{
		Remapping.Add(Binding.DefaultKey, Binding.Key);
	}
	if (UKnightInputConfig* Config = GetKnightInputConfig())
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

void AKnightCampaignPlayerController::HandleKeyRemap(const FName ActionId, const FKey Key)
{
	if (UKnightGameUserSettings* Settings = UKnightGameUserSettings::Get())
	{
		LastCampaignResult = Settings->RemapKey(ActionId, Key);
		Presentation.Feedback = LastCampaignResult.IsSuccess()
			? NSLOCTEXT("KnightCampaignUI", "Remapped", "Binding saved. Tutorial and interaction glyphs updated.")
			: LastCampaignResult.Message;
		RefreshCampaignPresentation();
	}
}

void AKnightCampaignPlayerController::HandleControlledCampaignKnight(AKnightCharacter* Previous, AKnightCharacter* Current)
{
	if (MovementKnight.IsValid())
	{
		MovementKnight->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::HandleMovementUpdated);
	}
	MovementKnight = Current;
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

void AKnightCampaignPlayerController::HandleMovementUpdated(const float DeltaSeconds,
	const FVector OldLocation, const FVector OldVelocity)
{
	UpdateMapPosition();
	if (UKnightGameUserSettings* Settings = UKnightGameUserSettings::Get(); Settings != nullptr
		&& !Settings->HasSeenTutorial(TEXT("Move")) && MovementKnight.IsValid()
		&& FVector::DistSquared(MovementKnight->GetActorLocation(), OnboardingOrigin) >= FMath::Square(200.0))
	{
		Settings->ConfirmTutorial(TEXT("Move"));
		RefreshCampaignPresentation();
	}
}

void AKnightCampaignPlayerController::UpdateMapPosition()
{
	UpdateInteractionHint();
	if (CampaignWorld == nullptr || !MovementKnight.IsValid())
	{
		return;
	}
	AKnightStoryState* State = GetWorld()->GetGameState<AKnightStoryState>();
	const FKnightMapRegion* Region = CampaignWorld->FindRegion(SelectedRegion);
	FVector2D UV = FVector2D::ZeroVector;
	const bool bVisible = State != nullptr && Region != nullptr
		&& SelectedRegion == State->WorldState->GetActiveRegionId()
		&& Region->Project(MovementKnight->GetActorLocation() + FVector(GetWorld()->OriginLocation), UV);
	Presentation.PlayerUV = UV;
	Presentation.bPlayerVisible = bVisible;
	const FKnightMapRegion* ActiveRegion = State != nullptr
		? CampaignWorld->FindRegion(State->WorldState->GetActiveRegionId()) : nullptr;
	Hud.bPlayerVisible = ActiveRegion != nullptr && ActiveRegion->Project(
		MovementKnight->GetActorLocation() + FVector(GetWorld()->OriginLocation), Hud.PlayerUV);
	if (UKnightRPGPanel* Panel = GetRPGPanel())
	{
		Panel->UpdateMapPosition(UV, bVisible);
	}
}

const FKnightCampaignPanelSnapshot& AKnightCampaignPlayerController::GetCampaignPresentation() const
{
	return Presentation;
}

const FKnightCampaignHudView& AKnightCampaignPlayerController::GetCampaignHud() const
{
	return Hud;
}

const FKnightResult& AKnightCampaignPlayerController::GetLastCampaignResult() const
{
	return LastCampaignResult;
}

void AKnightCampaignPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	bEndingCampaign = true;
	RemoveThreatSources();
	IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().Remove(InputDeviceHandle);
	OnCommandResult.Remove(GameplayFeedbackHandle);
	if (UKnightCampaignGameInstance* Session = Cast<UKnightCampaignGameInstance>(GetGameInstance()))
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
	if (MovementKnight.IsValid())
	{
		MovementKnight->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::HandleMovementUpdated);
	}
	if (AKnightStoryState* State = GetWorld()->GetGameState<AKnightStoryState>())
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
	OnControlledKnightChanged.Remove(CampaignControlHandle);
	OnCompanionCommandResult.Remove(CompanionTutorialHandle);
	if (UKnightGameUserSettings* Settings = UKnightGameUserSettings::Get())
	{
		Settings->OnPreferencesChanged.Remove(SettingsHandle);
	}
	if (UKnightRPGPanel* Panel = GetRPGPanel())
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
