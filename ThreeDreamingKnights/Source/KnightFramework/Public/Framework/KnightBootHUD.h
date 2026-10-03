#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "KnightBootHUD.generated.h"

class AKnightBootGameMode;

/** Minimal foundation presentation; it reads the boot result, never owns gameplay state. */
UCLASS()
class KNIGHTFRAMEWORK_API AKnightBootHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleFoundationReady(const TArray<FText>& InStatus, bool bInIsReady);

	UPROPERTY(Transient)
	TArray<FText> CachedStatus;

	TWeakObjectPtr<AKnightBootGameMode> BootSource;
	FDelegateHandle FoundationReadyHandle;
	bool bIsReady = false;
};
