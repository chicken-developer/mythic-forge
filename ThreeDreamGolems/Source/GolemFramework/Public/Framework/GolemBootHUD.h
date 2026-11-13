#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GolemBootHUD.generated.h"

class AGolemBootGameMode;

/** Minimal foundation presentation; it reads the boot result, never owns gameplay state. */
UCLASS()
class GOLEMFRAMEWORK_API AGolemBootHUD : public AHUD
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

	TWeakObjectPtr<AGolemBootGameMode> BootSource;
	FDelegateHandle FoundationReadyHandle;
	bool bIsReady = false;
};
