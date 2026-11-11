#include "Training/GolemTrainingHUD.h"

#include "Character/GolemCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/GolemGameState.h"
#include "Party/GolemPartyState.h"
#include "Player/GolemPlayerController.h"
#include "Settings/GolemSettings.h"
#include "Types/GolemResult.h"

FText AGolemTrainingHUD::GetDisplayedGolemName() const
{
	return GolemName;
}

bool AGolemTrainingHUD::IsDisplayingCompletion() const
{
	return bCompleted;
}

void AGolemTrainingHUD::BeginPlay()
{
	Super::BeginPlay();
	GameStateHandle = GetWorld()->GameStateSetEvent.AddUObject(this, &AGolemTrainingHUD::HandleGameStateSet);
	HandleGameStateSet(GetWorld()->GetGameState());
	ControlSource = Cast<AGolemPlayerController>(PlayerOwner);
	if (ControlSource.IsValid())
	{
		HandleControlledGolemChanged(nullptr, ControlSource->GetControlledGolem());
		HandleCommandResult(ControlSource->GetLastCommandResult());
		ControlHandle = ControlSource->OnControlledGolemChanged.AddUObject(this,
			&AGolemTrainingHUD::HandleControlledGolemChanged);
		CommandHandle = ControlSource->OnCommandResult.AddUObject(this, &AGolemTrainingHUD::HandleCommandResult);
	}
}

void AGolemTrainingHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GameStateSetEvent.Remove(GameStateHandle);
	UnbindSources();
	Super::EndPlay(EndPlayReason);
}

void AGolemTrainingHUD::HandleGameStateSet(AGameStateBase* InGameState)
{
	if (TrainingSource.IsValid())
	{
		TrainingSource->OnPartyStateChanged.Remove(PartySourceHandle);
		TrainingSource->OnTrainingProgressChanged.Remove(ProgressHandle);
	}
	PartySourceHandle.Reset();
	ProgressHandle.Reset();
	TrainingSource = Cast<AGolemGameState>(InGameState);
	HandlePartyStateChanged(TrainingSource.IsValid() ? TrainingSource->GetPartyState() : nullptr);
	HandleTrainingProgress(TrainingSource.IsValid() ? TrainingSource->GetTrainingProgress() : FGolemTrainingProgress());
	if (TrainingSource.IsValid())
	{
		PartySourceHandle = TrainingSource->OnPartyStateChanged.AddUObject(this,
			&AGolemTrainingHUD::HandlePartyStateChanged);
		ProgressHandle = TrainingSource->OnTrainingProgressChanged.AddUObject(this,
			&AGolemTrainingHUD::HandleTrainingProgress);
	}
}

void AGolemTrainingHUD::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas == nullptr)
	{
		return;
	}
	const float PanelWidth = FMath::Min(680.0f, static_cast<float>(Canvas->SizeX) - 40.0f);
	DrawRect(FLinearColor(0.015f, 0.025f, 0.05f, 0.88f), 20.0f, 20.0f, PanelWidth, 280.0f);
	const FLinearColor Accent = bCompleted ? FLinearColor::Green : FLinearColor(0.4f, 0.75f, 1.0f);
	const TArray<FText> Lines =
	{
		NSLOCTEXT("KnightTraining", "Title", "THREE DREAMING GOLEMS - Training yard"),
		FText::Format(NSLOCTEXT("KnightTraining", "Controlled", "Controlled Golem: {0}"), GolemName),
		PartyStatus,
		ProgressStatus,
		NSLOCTEXT("KnightTraining", "Keyboard", "WASD move | Mouse look | Space jump | Tab / 1-3 switch"),
		NSLOCTEXT("KnightTraining", "Gamepad", "Pad: left/right stick | A jump | RB switch | Y restart"),
		NSLOCTEXT("KnightTraining", "Goal", "Walk through all 3 markers and control all 3 Golems."),
		bCompleted ? NSLOCTEXT("KnightTraining", "Won", "DRILL COMPLETE - press R to try again")
			: NSLOCTEXT("KnightTraining", "Retry", "Press R to restart the drill (host in co-op)."),
		CommandStatus
	};
	float TextY = 32.0f;
	for (const FText& Line : Lines)
	{
		DrawText(Line.ToString(), Accent, 34.0f, TextY, GEngine->GetSmallFont(), 1.15f);
		TextY += 28.0f;
	}
}

void AGolemTrainingHUD::HandlePartyStateChanged(AGolemPartyState* InPartyState)
{
	if (PartySource.IsValid())
	{
		PartySource->OnRosterChanged.Remove(RosterHandle);
	}
	RosterHandle.Reset();
	PartySource = InPartyState;
	HandleRosterChanged();
	if (PartySource.IsValid())
	{
		RosterHandle = PartySource->OnRosterChanged.AddUObject(this, &AGolemTrainingHUD::HandleRosterChanged);
	}
}

void AGolemTrainingHUD::HandleRosterChanged()
{
	const int32 HumanCount = PartySource.IsValid() ? PartySource->GetHumanCount() : 0;
	const int32 GolemCount = PartySource.IsValid() ? PartySource->GetRoster().Num() : 0;
	PartyStatus = FText::Format(NSLOCTEXT("KnightTraining", "PartyStatus", "Party: {0}/3 Golems | {1} human | {2} AI"),
		FText::AsNumber(GolemCount), FText::AsNumber(HumanCount), FText::AsNumber(GolemCount - HumanCount));
}

void AGolemTrainingHUD::HandleControlledGolemChanged(AGolemCharacter* PreviousGolem,
	AGolemCharacter* CurrentGolem)
{
	if (GolemSource.IsValid())
	{
		GolemSource->OnIdentityChanged.Remove(IdentityHandle);
	}
	IdentityHandle.Reset();
	GolemSource = CurrentGolem;
	HandleIdentityChanged();
	if (GolemSource.IsValid())
	{
		IdentityHandle = GolemSource->OnIdentityChanged.AddUObject(this, &AGolemTrainingHUD::HandleIdentityChanged);
	}
}

void AGolemTrainingHUD::HandleIdentityChanged()
{
	GolemName = GolemSource.IsValid() ? GolemSource->GetGolemDisplayName()
		: NSLOCTEXT("KnightTraining", "WaitingGolem", "Waiting for possession");
}

void AGolemTrainingHUD::HandleTrainingProgress(const FGolemTrainingProgress& Progress)
{
	int32 ControlledCount = 0;
	int32 MarkerCount = 0;
	for (int32 Slot = 0; Slot < UGolemSettings::CanonicalPartySize; ++Slot)
	{
		ControlledCount += (Progress.ControlledMask & (1 << Slot)) != 0 ? 1 : 0;
	}
	for (int32 Marker = 0; Marker < AGolemGameState::RequiredMarkerCount; ++Marker)
	{
		MarkerCount += (Progress.MarkerMask & (1 << Marker)) != 0 ? 1 : 0;
	}
	bCompleted = Progress.bCompleted;
	ProgressStatus = FText::Format(NSLOCTEXT("KnightTraining", "Progress", "Golems controlled: {0}/3 | Markers: {1}/3"),
		FText::AsNumber(ControlledCount), FText::AsNumber(MarkerCount));
}

void AGolemTrainingHUD::HandleCommandResult(const FGolemResult& Result)
{
	CommandStatus = Result.IsSuccess() ? FText::GetEmpty() : Result.Message;
}

void AGolemTrainingHUD::UnbindSources()
{
	if (TrainingSource.IsValid())
	{
		TrainingSource->OnPartyStateChanged.Remove(PartySourceHandle);
		TrainingSource->OnTrainingProgressChanged.Remove(ProgressHandle);
	}
	if (PartySource.IsValid())
	{
		PartySource->OnRosterChanged.Remove(RosterHandle);
	}
	if (ControlSource.IsValid())
	{
		ControlSource->OnControlledGolemChanged.Remove(ControlHandle);
		ControlSource->OnCommandResult.Remove(CommandHandle);
	}
	if (GolemSource.IsValid())
	{
		GolemSource->OnIdentityChanged.Remove(IdentityHandle);
	}
	TrainingSource.Reset();
	PartySource.Reset();
	ControlSource.Reset();
	GolemSource.Reset();
}
