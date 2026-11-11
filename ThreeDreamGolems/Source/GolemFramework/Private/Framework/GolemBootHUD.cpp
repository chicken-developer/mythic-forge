#include "Framework/GolemBootHUD.h"

#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Framework/GolemBootGameMode.h"

void AGolemBootHUD::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas == nullptr)
	{
		return;
	}
	DrawRect(FLinearColor(0.015f, 0.02f, 0.035f, 0.94f), 20.0f, 20.0f, 700.0f, 440.0f);
	DrawText(NSLOCTEXT("GolemBoot", "Title", "Three Dream Golems - Foundation").ToString(),
		FLinearColor::White, 40.0f, 40.0f, nullptr, 1.5f);
	float RowY = 85.0f;
	for (const FText& Status : CachedStatus)
	{
		DrawText(Status.ToString(), FLinearColor::White, 40.0f, RowY);
		RowY += 20.0f;
	}
	DrawText(bIsReady
		? NSLOCTEXT("GolemBoot", "Ready", "PASS - stop and play again to check restart").ToString()
		: NSLOCTEXT("GolemBoot", "Failed", "FAIL - inspect LogGolemBoot and asset validation").ToString(),
		bIsReady ? FLinearColor::Green : FLinearColor::Red, 40.0f, RowY + 15.0f);
}

void AGolemBootHUD::BeginPlay()
{
	Super::BeginPlay();
	BootSource = GetWorld()->GetAuthGameMode<AGolemBootGameMode>();
	if (BootSource.IsValid())
	{
		HandleFoundationReady(BootSource->GetFoundationStatus(), BootSource->IsFoundationReady());
		FoundationReadyHandle = BootSource->OnFoundationReady.AddUObject(this,
			&AGolemBootHUD::HandleFoundationReady);
	}
}

void AGolemBootHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BootSource.IsValid())
	{
		BootSource->OnFoundationReady.Remove(FoundationReadyHandle);
	}
	FoundationReadyHandle.Reset();
	BootSource.Reset();
	Super::EndPlay(EndPlayReason);
}

void AGolemBootHUD::HandleFoundationReady(const TArray<FText>& InStatus, bool bInIsReady)
{
	CachedStatus = InStatus;
	bIsReady = bInIsReady;
}
