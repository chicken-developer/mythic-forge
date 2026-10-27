#include "Story/KnightStoryHUD.h"

#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Story/KnightStoryPlayerController.h"
#include "Story/KnightStoryState.h"

void AKnightStoryHUD::DrawHUD()
{
	if (Canvas == nullptr)
	{
		return;
	}
	const AKnightStoryPlayerController* StoryPlayer = Cast<AKnightStoryPlayerController>(GetOwningPlayerController());
	const AKnightStoryState* State = GetWorld()->GetGameState<AKnightStoryState>();
	if (State != nullptr && State->Quest->GetSnapshot().ScenarioKind == TEXT("DeveloperFixture"))
	{
		Super::DrawHUD();
	}
	else
	{
		AHUD::DrawHUD();
		DrawText(GetArenaTitle().ToString(), FLinearColor::Yellow, 25.0f, 25.0f, nullptr, 1.2f);
		DrawText(GetCombatHelp().ToString(), FLinearColor::White, 25.0f, 55.0f);
		const AKnightCharacter* ControlledKnight = StoryPlayer != nullptr ? StoryPlayer->GetControlledKnight() : nullptr;
		if (ControlledKnight != nullptr && ControlledKnight->GetKnightAttributeSet() != nullptr)
		{
			DrawText(FString::Printf(TEXT("%s | HP %.0f/%.0f"), *ControlledKnight->GetKnightDisplayName().ToString(),
				ControlledKnight->GetKnightAttributeSet()->GetHealth(),
				ControlledKnight->GetKnightAttributeSet()->GetMaxHealth()),
				FLinearColor::White, 25.0f, 80.0f);
		}
	}
	if (Canvas != nullptr && StoryPlayer != nullptr)
	{
		DrawText(StoryPlayer->GetStoryStatus().ToString(), FLinearColor::White, 25.0f, 300.0f, nullptr, 1.0f);
	}
}

FText AKnightStoryHUD::GetArenaTitle() const
{
	return NSLOCTEXT("KnightStory", "DemoTitle", "Boro's forge school — Story framework demo");
}

FText AKnightStoryHUD::GetCombatHelp() const
{
	return NSLOCTEXT("KnightStory", "DemoHelp", "WASD / mouse: move. F: nearby station / next dialogue. I: inventory / save / Continue.");
}
