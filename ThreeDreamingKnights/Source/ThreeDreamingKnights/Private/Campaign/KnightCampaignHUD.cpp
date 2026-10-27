#include "Campaign/KnightCampaignHUD.h"

#include "Campaign/KnightCampaignPlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

void AKnightCampaignHUD::DrawHUD()
{
	Super::DrawHUD();
	const AKnightCampaignPlayerController* Player = Cast<AKnightCampaignPlayerController>(GetOwningPlayerController());
	if (Canvas == nullptr || Player == nullptr)
	{
		return;
	}
	const FKnightCampaignHudView& View = Player->GetCampaignHud();
	const float Scale = FMath::Clamp(Canvas->SizeY / 900.0f, 0.7f, 1.5f) * View.TextScale;
	float Y = 18.0f;
	for (const FText& Line : View.PartyLines)
	{
		Y = DrawWrapped(Line, 22.0f, Y, Canvas->SizeX - 245.0f * Scale, Scale, FLinearColor::White);
	}
	Y = DrawWrapped(View.Tutorial, 22.0f, Y + 16.0f, Canvas->SizeX - 50.0f, Scale, FLinearColor::White);
	Y = DrawWrapped(View.Interaction, 22.0f, Y + 8.0f, Canvas->SizeX - 50.0f, Scale, FLinearColor::Yellow);
	Y = DrawWrapped(View.Target, 22.0f, Y + 8.0f, Canvas->SizeX - 50.0f, Scale, FLinearColor::White);
	DrawWrapped(View.Companion, 22.0f, Canvas->SizeY - 50.0f * Scale, Canvas->SizeX - 50.0f, Scale, FLinearColor::White);
	const double Now = GetWorld()->GetGameState() != nullptr
		? GetWorld()->GetGameState()->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	if (Now < View.CombatFeedbackEnd)
	{
		Y = DrawWrapped(View.CombatFeedback, 22.0f, Y, Canvas->SizeX - 50.0f, Scale, FLinearColor::Yellow);
	}
	for (const FKnightCampaignThreatView& Threat : View.Threats)
	{
		if (Threat.EndTime > Now)
		{
			Y = DrawWrapped(Threat.Text, 22.0f, Y, Canvas->SizeX - 50.0f, Scale, FLinearColor::Yellow);
		}
	}
	float X = 30.0f;
	for (const FKnightCampaignCooldownView& Cooldown : View.Cooldowns)
	{
		const float Remaining = FMath::Max(0.0, Cooldown.EndTime - Now);
		DrawText(FString::Printf(TEXT("%s  %s"), *Cooldown.Label.ToString(),
			Remaining > 0.0f ? *FString::Printf(TEXT("%.1fs"), Remaining) : TEXT("Ready")),
			FLinearColor::White, X, Canvas->SizeY - 82.0f * Scale, nullptr, Scale);
		X += 140.0f * Scale;
	}
	const float MapX = Canvas->SizeX - 190.0f * Scale;
	DrawRect(FLinearColor(0.02f, 0.035f, 0.04f, 0.85f), MapX, 24.0f, 165.0f * Scale, 165.0f * Scale);
	for (const FVector2D& UV : View.MinimapMarkers)
	{
		DrawText(TEXT("o"), FLinearColor::White, MapX + UV.X * 150.0f * Scale,
			24.0f + UV.Y * 150.0f * Scale, nullptr, Scale);
	}
	if (View.bPlayerVisible)
	{
		DrawText(TEXT("YOU"), FLinearColor::Yellow, MapX + View.PlayerUV.X * 150.0f * Scale,
			24.0f + View.PlayerUV.Y * 150.0f * Scale, nullptr, Scale);
	}
	if (!View.Subtitle.IsEmpty())
	{
		if (View.bSubtitleBackground)
		{
			DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f), Canvas->SizeX * 0.12f,
				Canvas->SizeY * 0.78f, Canvas->SizeX * 0.76f, 70.0f * Scale * View.SubtitleScale);
		}
		DrawWrapped(View.Subtitle, Canvas->SizeX * 0.14f, Canvas->SizeY * 0.8f,
			Canvas->SizeX * 0.72f, Scale * View.SubtitleScale, FLinearColor::White);
	}
}

float AKnightCampaignHUD::DrawWrapped(const FText& Text, const float X, const float Y, const float Width,
	const float Scale, const FLinearColor Color)
{
	TArray<FString> Words;
	Text.ToString().ParseIntoArrayWS(Words);
	FString Line;
	float CurrentY = Y;
	for (const FString& Word : Words)
	{
		const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		GetTextSize(Candidate, TextWidth, TextHeight, nullptr, Scale);
		if (!Line.IsEmpty() && TextWidth > Width)
		{
			DrawText(Line, Color, X, CurrentY, nullptr, Scale);
			CurrentY += 23.0f * Scale;
			Line = Word;
		}
		else
		{
			Line = Candidate;
		}
	}
	if (!Line.IsEmpty())
	{
		DrawText(Line, Color, X, CurrentY, nullptr, Scale);
		CurrentY += 23.0f * Scale;
	}
	return CurrentY;
}
