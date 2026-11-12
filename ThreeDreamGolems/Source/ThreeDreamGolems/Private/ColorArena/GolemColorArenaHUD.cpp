#include "ColorArena/GolemColorArenaHUD.h"

#include "Components/GolemCombatComponent.h"
#include "Character/GolemCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Types/GolemElement.h"
#include "UObject/UObjectIterator.h"
#include "Voids/GolemColorVoidComponent.h"
#include "Voids/GolemVoidHostComponent.h"

FString AGolemColorArenaHUD::DescribeVoid(const UGolemColorVoidComponent* Void, const double HostNow)
{
	if (Void == nullptr)
	{
		return FString();
	}
	switch (Void->GetVoidState())
	{
	case EGolemVoidState::Open:
		return GolemElements::SymbolText(Void->GetMissingMask());
	case EGolemVoidState::Primed:
		return FString::Printf(TEXT("%s  %.1f"), *GolemElements::SymbolText(Void->GetMissingMask()),
			FMath::Max(0.0, Void->GetTimerEndServerTime() - HostNow));
	case EGolemVoidState::Dulled:
		return FString::Printf(TEXT("X  %.1f"), FMath::Max(0.0, Void->GetTimerEndServerTime() - HostNow));
	default:
		return FString();
	}
}

void AGolemColorArenaHUD::DrawHUD()
{
	Super::DrawHUD();
	const UWorld* World = GetWorld();
	if (Canvas == nullptr || World == nullptr)
	{
		return;
	}
	for (TObjectIterator<UGolemColorVoidComponent> It; It; ++It)
	{
		const UGolemColorVoidComponent* Void = *It;
		if (IsValid(Void) && Void->GetWorld() == World && Void->IsRegistered())
		{
			DrawVoid(Void, Void->GetHostTimeSeconds());
		}
	}
	// Progress of each Xenon reads as voids filled, never as a health bar.
	for (TObjectIterator<UGolemVoidHostComponent> It; It; ++It)
	{
		const UGolemVoidHostComponent* Host = *It;
		if (!IsValid(Host) || Host->GetWorld() != World || !IsValid(Host->GetOwner()) || Host->GetTotalFills() <= 0)
		{
			continue;
		}
		const FVector Screen = Project(Host->GetOwner()->GetActorLocation() + FVector(0.0, 0.0, 260.0));
		if (Screen.Z > 0.0)
		{
			const FString Label = Host->IsRestored() ? FString(TEXT("RESTORED"))
				: FString::Printf(TEXT("%d / %d"), Host->GetFilledCount(), Host->GetTotalFills());
			DrawText(Label, Host->IsRestored() ? FLinearColor(0.85f, 0.75f, 0.45f) : FLinearColor::White,
				Screen.X - 30.0f, Screen.Y, nullptr, 1.2f);
		}
	}
	const AGolemCharacter* OwnGolem = PlayerOwner != nullptr ? Cast<AGolemCharacter>(PlayerOwner->GetPawn()) : nullptr;
	if (OwnGolem != nullptr && OwnGolem->GetCombatComponent() != nullptr)
	{
		const float Cooldown = OwnGolem->GetCombatComponent()->GetDodgeCooldownRemaining(World->GetGameState() != nullptr
			? World->GetGameState()->GetServerWorldTimeSeconds() : World->GetTimeSeconds());
		DrawText(Cooldown > 0.0f ? FString::Printf(TEXT("Dodge %.1f"), Cooldown) : FString(TEXT("Dodge ready")),
			Cooldown > 0.0f ? FLinearColor(0.6f, 0.6f, 0.6f) : FLinearColor::Green, 25.0f, Canvas->SizeY - 60.0f, nullptr, 1.15f);
	}
}

void AGolemColorArenaHUD::DrawVoid(const UGolemColorVoidComponent* Void, const double HostNow)
{
	const FString Label = DescribeVoid(Void, HostNow);
	if (Label.IsEmpty())
	{
		return;
	}
	const bool bSignaled = Void->IsSignaledAt(HostNow);
	const FLinearColor Color = Void->GetVoidState() == EGolemVoidState::Dulled
		? FLinearColor(0.5f, 0.5f, 0.5f) : GolemElements::DisplayColor(Void->GetRequiredMask());
	const FVector Screen = Project(Void->GetComponentLocation());
	const bool bOnScreen = Screen.Z > 0.0 && Screen.X >= 0.0 && Screen.X <= Canvas->SizeX
		&& Screen.Y >= 0.0 && Screen.Y <= Canvas->SizeY;
	if (!bOnScreen)
	{
		if (bSignaled)
		{
			DrawEdgeArrow(Void->GetComponentLocation(), Color, GolemElements::SymbolText(Void->GetMissingMask()));
		}
		return;
	}
	float Width = 0.0f;
	float Height = 0.0f;
	GetTextSize(Label, Width, Height, nullptr, 1.6f);
	DrawText(Label, FLinearColor::Black, Screen.X - Width * 0.5f + 1.5f, Screen.Y - Height * 0.5f + 1.5f, nullptr, 1.6f);
	DrawText(Label, Color, Screen.X - Width * 0.5f, Screen.Y - Height * 0.5f, nullptr, 1.6f);
	if (Void->GetVoidState() == EGolemVoidState::Primed)
	{
		const FGolemVoidTuning& Tuning = Void->GetTuning();
		const float Window = Void->GetRequiredMask() == GolemElements::AllMask ? Tuning.WhiteCoreWindowSeconds : Tuning.MixWindowSeconds;
		const float Remaining = static_cast<float>(Void->GetTimerEndServerTime() - HostNow);
		// ColorHold can make the window longer than the base value; the ring is simply full until then.
		DrawCountdownRing(FVector2D(Screen.X, Screen.Y), 34.0f, FMath::Clamp(Remaining / FMath::Max(Window, 0.1f), 0.0f, 1.0f),
			GolemElements::DisplayColor(Void->GetAppliedMask()));
	}
	if (bSignaled)
	{
		// The marker: a white frame around the void and the symbols still needed above it.
		const float Half = 46.0f;
		DrawLine(Screen.X - Half, Screen.Y - Half, Screen.X + Half, Screen.Y - Half, FLinearColor::White, 3.0f);
		DrawLine(Screen.X + Half, Screen.Y - Half, Screen.X + Half, Screen.Y + Half, FLinearColor::White, 3.0f);
		DrawLine(Screen.X + Half, Screen.Y + Half, Screen.X - Half, Screen.Y + Half, FLinearColor::White, 3.0f);
		DrawLine(Screen.X - Half, Screen.Y + Half, Screen.X - Half, Screen.Y - Half, FLinearColor::White, 3.0f);
		DrawText(FString::Printf(TEXT("! %s"), *GolemElements::SymbolText(Void->GetMissingMask())), FLinearColor::White,
			Screen.X - 22.0f, Screen.Y - Half - 28.0f, nullptr, 1.5f);
	}
}

void AGolemColorArenaHUD::DrawCountdownRing(const FVector2D& Center, const float Radius, const float Fraction,
	const FLinearColor& Color)
{
	const int32 Segments = 32;
	const int32 Lit = FMath::CeilToInt(Segments * Fraction);
	for (int32 Index = 0; Index < Lit; ++Index)
	{
		// Starts at twelve o'clock and shrinks clockwise.
		const float A = -UE_HALF_PI + UE_TWO_PI * Index / Segments;
		const float B = -UE_HALF_PI + UE_TWO_PI * (Index + 1) / Segments;
		DrawLine(Center.X + Radius * FMath::Cos(A), Center.Y + Radius * FMath::Sin(A),
			Center.X + Radius * FMath::Cos(B), Center.Y + Radius * FMath::Sin(B), Color, 4.0f);
	}
}

void AGolemColorArenaHUD::DrawEdgeArrow(const FVector& WorldLocation, const FLinearColor& Color, const FString& Label)
{
	FVector ViewLocation;
	FRotator ViewRotation;
	if (PlayerOwner == nullptr)
	{
		return;
	}
	PlayerOwner->GetPlayerViewPoint(ViewLocation, ViewRotation);
	// Direction on screen: right and up components of the offset in view space.
	const FVector Local = ViewRotation.UnrotateVector(WorldLocation - ViewLocation);
	FVector2D Direction(Local.Y, -Local.Z);
	if (Direction.IsNearlyZero())
	{
		Direction = FVector2D(0.0, 1.0);
	}
	Direction.Normalize();
	const FVector2D Center(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f);
	const float Margin = 60.0f;
	const float Scale = FMath::Min((Center.X - Margin) / FMath::Max(FMath::Abs(Direction.X), 0.001),
		(Center.Y - Margin) / FMath::Max(FMath::Abs(Direction.Y), 0.001));
	const FVector2D Tip = Center + Direction * Scale;
	const FVector2D Back = Tip - Direction * 28.0f;
	const FVector2D Side(-Direction.Y * 12.0f, Direction.X * 12.0f);
	DrawLine(Tip.X, Tip.Y, Back.X + Side.X, Back.Y + Side.Y, FLinearColor::White, 3.0f);
	DrawLine(Tip.X, Tip.Y, Back.X - Side.X, Back.Y - Side.Y, FLinearColor::White, 3.0f);
	DrawLine(Back.X + Side.X, Back.Y + Side.Y, Back.X - Side.X, Back.Y - Side.Y, FLinearColor::White, 3.0f);
	DrawText(FString::Printf(TEXT("! %s"), *Label), Color, Back.X - Direction.X * 30.0f - 14.0f, Back.Y - Direction.Y * 30.0f - 10.0f,
		nullptr, 1.4f);
}

FText AGolemColorArenaHUD::GetArenaTitle() const
{
	return NSLOCTEXT("GolemColorArena", "Title", "COLOUR ARENA: fill every glowing void to restore the Xenons");
}

FText AGolemColorArenaHUD::GetObjectiveText() const
{
	int32 Total = 0;
	int32 Restored = 0;
	for (TObjectIterator<UGolemVoidHostComponent> It; It; ++It)
	{
		if (IsValid(*It) && It->GetWorld() == GetWorld() && It->GetTotalFills() > 0)
		{
			++Total;
			Restored += It->IsRestored() ? 1 : 0;
		}
	}
	return Total > 0 && Restored == Total
		? NSLOCTEXT("GolemColorArena", "AllRestored", "Every Xenon is restored. R / Y: host restarts with fresh Xenons.")
		: FText::Format(NSLOCTEXT("GolemColorArena", "Objective",
			"Xenons restored: {0} / {1}. A void shows the elements it still needs: ^ Earth (Mars), O Sky (Jupiter), ~ Sea (Neptune)."),
			FText::AsNumber(Restored), FText::AsNumber(Total));
}

FText AGolemColorArenaHUD::GetCombatHelp() const
{
	return NSLOCTEXT("GolemColorArena", "CombatKeys",
		"WASD move | LMB light (no colour) | RMB heavy: places your colour on a void | Shift dodge | Q block");
}

FText AGolemColorArenaHUD::GetPartyHelp() const
{
	return NSLOCTEXT("GolemColorArena", "PartyKeys",
		"X / D-pad Up: signal the void you look at, companions add their colour | Tab / RB switch Golem | E / pad X hold revive");
}

FText AGolemColorArenaHUD::GetRecoveryHelp() const
{
	return NSLOCTEXT("GolemColorArena", "Rules",
		"Wrong colour locks a void for 3 s. A started mix must be finished within 4 s. The grey body takes only 10% damage.");
}
