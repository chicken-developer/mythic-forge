#include "Story/GolemStoryHUD.h"

#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "Story/GolemStoryPlayerController.h"
#include "Story/GolemStoryState.h"

void AGolemStoryHUD::DrawHUD()
{
	if (Canvas == nullptr)
	{
		return;
	}
	const AGolemStoryPlayerController* StoryPlayer = Cast<AGolemStoryPlayerController>(GetOwningPlayerController());
	const AGolemStoryState* State = GetWorld()->GetGameState<AGolemStoryState>();
	if (State != nullptr && State->Quest->GetSnapshot().ScenarioKind == TEXT("DeveloperFixture"))
	{
		Super::DrawHUD();
	}
	else
	{
		AHUD::DrawHUD();
		DrawText(GetArenaTitle().ToString(), FLinearColor::Yellow, 25.0f, 25.0f, nullptr, 1.2f);
		DrawText(GetCombatHelp().ToString(), FLinearColor::White, 25.0f, 55.0f);
		const AGolemCharacter* ControlledGolem = StoryPlayer != nullptr ? StoryPlayer->GetControlledGolem() : nullptr;
		if (ControlledGolem != nullptr && ControlledGolem->GetGolemAttributeSet() != nullptr)
		{
			DrawText(FString::Printf(TEXT("%s | HP %.0f/%.0f"), *ControlledGolem->GetGolemDisplayName().ToString(),
				ControlledGolem->GetGolemAttributeSet()->GetHealth(),
				ControlledGolem->GetGolemAttributeSet()->GetMaxHealth()),
				FLinearColor::White, 25.0f, 80.0f);
		}
	}
	if (Canvas != nullptr && StoryPlayer != nullptr)
	{
		DrawText(StoryPlayer->GetStoryStatus().ToString(), FLinearColor::White, 25.0f, 300.0f, nullptr, 1.0f);
	}
}

FText AGolemStoryHUD::GetArenaTitle() const
{
	return NSLOCTEXT("KnightStory", "DemoTitle", "Boro's forge school — Story framework demo");
}

FText AGolemStoryHUD::GetCombatHelp() const
{
	return NSLOCTEXT("KnightStory", "DemoHelp", "WASD / mouse: move. F: nearby station / next dialogue. I: inventory / save / Continue.");
}
