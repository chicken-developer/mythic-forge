#include "GearLab/KnightGearLabHUD.h"

#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GearLab/KnightGearLabState.h"
#include "RPGLoop/KnightRPGUIPlayerController.h"
#include "TimerManager.h"

void AKnightGearLabHUD::BeginPlay()
{
	Super::BeginPlay();
	BindGearSources();
}

void AKnightGearLabHUD::BindGearSources()
{
	GearController = Cast<AKnightRPGUIPlayerController>(GetOwningPlayerController());
	for (TActorIterator<AKnightGearLabState> It(GetWorld()); It; ++It)
	{
		GearState = *It;
		break;
	}
	if (!GearController.IsValid() || !GearState.IsValid())
	{
		if (++Attempts < 100)
		{
			GetWorldTimerManager().SetTimer(ReadinessTimer, this, &ThisClass::BindGearSources, 0.1f, false);
		}
		return;
	}
	PresentationHandle = GearController->OnRPGPresentationChanged.AddUObject(this, &ThisClass::RefreshGearHUD);
	FixtureHandle = GearState->OnGearDemoChanged.AddWeakLambda(this,
		[this](const FKnightGearDemoSnapshot& State)
		{
			RefreshGearHUD();
		});
	RefreshGearHUD();
}

void AKnightGearLabHUD::RefreshGearHUD()
{
	QuickUses = GearController.IsValid() ? GearController->GetQuickUseReadModel() : TArray<FKnightGearQuickUseRow>();
	Objective = GearState.IsValid() ? GearState->GetSnapshot().StageLabel : FText::GetEmpty();
	GearTitle = GearState.IsValid() && GearState->GetSnapshot().bCatalogLab
		? NSLOCTEXT("KnightGearLab", "CatalogHUDTitle", "M5.6 EQUIPMENT: DEV Catalog Lab")
		: NSLOCTEXT("KnightGearLab", "ProgressionHUDTitle", "M5.6 EQUIPMENT: Progression Demo A / B / C");
}

FText AKnightGearLabHUD::GetArenaTitle() const
{
	return GearTitle;
}

void AKnightGearLabHUD::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas == nullptr)
	{
		return;
	}
	const AGameStateBase* State = GetWorld()->GetGameState();
	const double Now = State != nullptr ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	FString Line;
	for (int32 Index = 0; Index < QuickUses.Num(); ++Index)
	{
		Line += FString::Printf(TEXT("Q%d [%d]: %s x%d | shared CD %.1fs    "), Index + 1, Index + 1,
			*QuickUses[Index].Name.ToString(), QuickUses[Index].Count,
			FMath::Max(0.0, QuickUses[Index].CooldownEndTime - Now));
	}
	// Bottom anchored lines leave the existing skill/debt and Runestone HUD unobstructed.
	DrawText(Line, FLinearColor::Green, 25.0f, Canvas->SizeY - 68.0f, nullptr, 0.85f);
	DrawText(Objective.ToString(), FLinearColor::White, 25.0f, Canvas->SizeY - 43.0f, nullptr, 0.85f);
}

void AKnightGearLabHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(ReadinessTimer);
	if (GearController.IsValid())
	{
		GearController->OnRPGPresentationChanged.Remove(PresentationHandle);
	}
	if (GearState.IsValid())
	{
		GearState->OnGearDemoChanged.Remove(FixtureHandle);
	}
	Super::EndPlay(Reason);
}
