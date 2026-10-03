#include "Training/KnightTrainingHUD.h"

#include "Character/KnightCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/KnightGameState.h"
#include "Party/KnightPartyState.h"
#include "Player/KnightPlayerController.h"
#include "Settings/KnightSettings.h"
#include "Types/KnightResult.h"

FText AKnightTrainingHUD::GetDisplayedKnightName() const
{
	return KnightName;
}

bool AKnightTrainingHUD::IsDisplayingCompletion() const
{
	return bCompleted;
}

void AKnightTrainingHUD::BeginPlay()
{
	Super::BeginPlay();
	GameStateHandle = GetWorld()->GameStateSetEvent.AddUObject(this, &AKnightTrainingHUD::HandleGameStateSet);
	HandleGameStateSet(GetWorld()->GetGameState());
	ControlSource = Cast<AKnightPlayerController>(PlayerOwner);
	if (ControlSource.IsValid())
	{
		HandleControlledKnightChanged(nullptr, ControlSource->GetControlledKnight());
		HandleCommandResult(ControlSource->GetLastCommandResult());
		ControlHandle = ControlSource->OnControlledKnightChanged.AddUObject(this,
			&AKnightTrainingHUD::HandleControlledKnightChanged);
		CommandHandle = ControlSource->OnCommandResult.AddUObject(this, &AKnightTrainingHUD::HandleCommandResult);
	}
}

void AKnightTrainingHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GameStateSetEvent.Remove(GameStateHandle);
	UnbindSources();
	Super::EndPlay(EndPlayReason);
}

void AKnightTrainingHUD::HandleGameStateSet(AGameStateBase* InGameState)
{
	if (TrainingSource.IsValid())
	{
		TrainingSource->OnPartyStateChanged.Remove(PartySourceHandle);
		TrainingSource->OnTrainingProgressChanged.Remove(ProgressHandle);
	}
	PartySourceHandle.Reset();
	ProgressHandle.Reset();
	TrainingSource = Cast<AKnightGameState>(InGameState);
	HandlePartyStateChanged(TrainingSource.IsValid() ? TrainingSource->GetPartyState() : nullptr);
	HandleTrainingProgress(TrainingSource.IsValid() ? TrainingSource->GetTrainingProgress() : FKnightTrainingProgress());
	if (TrainingSource.IsValid())
	{
		PartySourceHandle = TrainingSource->OnPartyStateChanged.AddUObject(this,
			&AKnightTrainingHUD::HandlePartyStateChanged);
		ProgressHandle = TrainingSource->OnTrainingProgressChanged.AddUObject(this,
			&AKnightTrainingHUD::HandleTrainingProgress);
	}
}

void AKnightTrainingHUD::DrawHUD()
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
		NSLOCTEXT("KnightTraining", "Title", "THREE DREAMING KNIGHTS - Training yard"),
		FText::Format(NSLOCTEXT("KnightTraining", "Controlled", "Controlled Knight: {0}"), KnightName),
		PartyStatus,
		ProgressStatus,
		NSLOCTEXT("KnightTraining", "Keyboard", "WASD move | Mouse look | Space jump | Tab / 1-3 switch"),
		NSLOCTEXT("KnightTraining", "Gamepad", "Pad: left/right stick | A jump | RB switch | Y restart"),
		NSLOCTEXT("KnightTraining", "Goal", "Walk through all 3 markers and control all 3 Knights."),
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

void AKnightTrainingHUD::HandlePartyStateChanged(AKnightPartyState* InPartyState)
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
		RosterHandle = PartySource->OnRosterChanged.AddUObject(this, &AKnightTrainingHUD::HandleRosterChanged);
	}
}

void AKnightTrainingHUD::HandleRosterChanged()
{
	const int32 HumanCount = PartySource.IsValid() ? PartySource->GetHumanCount() : 0;
	const int32 KnightCount = PartySource.IsValid() ? PartySource->GetRoster().Num() : 0;
	PartyStatus = FText::Format(NSLOCTEXT("KnightTraining", "PartyStatus", "Party: {0}/3 Knights | {1} human | {2} AI"),
		FText::AsNumber(KnightCount), FText::AsNumber(HumanCount), FText::AsNumber(KnightCount - HumanCount));
}

void AKnightTrainingHUD::HandleControlledKnightChanged(AKnightCharacter* PreviousKnight,
	AKnightCharacter* CurrentKnight)
{
	if (KnightSource.IsValid())
	{
		KnightSource->OnIdentityChanged.Remove(IdentityHandle);
	}
	IdentityHandle.Reset();
	KnightSource = CurrentKnight;
	HandleIdentityChanged();
	if (KnightSource.IsValid())
	{
		IdentityHandle = KnightSource->OnIdentityChanged.AddUObject(this, &AKnightTrainingHUD::HandleIdentityChanged);
	}
}

void AKnightTrainingHUD::HandleIdentityChanged()
{
	KnightName = KnightSource.IsValid() ? KnightSource->GetKnightDisplayName()
		: NSLOCTEXT("KnightTraining", "WaitingKnight", "Waiting for possession");
}

void AKnightTrainingHUD::HandleTrainingProgress(const FKnightTrainingProgress& Progress)
{
	int32 ControlledCount = 0;
	int32 MarkerCount = 0;
	for (int32 Slot = 0; Slot < UKnightSettings::CanonicalPartySize; ++Slot)
	{
		ControlledCount += (Progress.ControlledMask & (1 << Slot)) != 0 ? 1 : 0;
	}
	for (int32 Marker = 0; Marker < AKnightGameState::RequiredMarkerCount; ++Marker)
	{
		MarkerCount += (Progress.MarkerMask & (1 << Marker)) != 0 ? 1 : 0;
	}
	bCompleted = Progress.bCompleted;
	ProgressStatus = FText::Format(NSLOCTEXT("KnightTraining", "Progress", "Knights controlled: {0}/3 | Markers: {1}/3"),
		FText::AsNumber(ControlledCount), FText::AsNumber(MarkerCount));
}

void AKnightTrainingHUD::HandleCommandResult(const FKnightResult& Result)
{
	CommandStatus = Result.IsSuccess() ? FText::GetEmpty() : Result.Message;
}

void AKnightTrainingHUD::UnbindSources()
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
		ControlSource->OnControlledKnightChanged.Remove(ControlHandle);
		ControlSource->OnCommandResult.Remove(CommandHandle);
	}
	if (KnightSource.IsValid())
	{
		KnightSource->OnIdentityChanged.Remove(IdentityHandle);
	}
	TrainingSource.Reset();
	PartySource.Reset();
	ControlSource.Reset();
	KnightSource.Reset();
}
