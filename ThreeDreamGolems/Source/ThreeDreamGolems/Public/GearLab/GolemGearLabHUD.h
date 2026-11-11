#pragma once

#include "CoreMinimal.h"
#include "SkillLab/GolemSkillLabHUD.h"
#include "Models/GolemGearPanelTypes.h"
#include "GolemGearLabHUD.generated.h"

class AGolemGearLabState;
class AGolemRPGUIPlayerController;

/** Event-bound quick-use and fixture presentation layered on the M5.5 HUD. */
UCLASS()
class THREEDREAMGOLEMS_API AGolemGearLabHUD : public AGolemSkillLabHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual FText GetArenaTitle() const override;
private:
	void BindGearSources();
	void RefreshGearHUD();
	TWeakObjectPtr<AGolemRPGUIPlayerController> GearController;
	TWeakObjectPtr<AGolemGearLabState> GearState;
	FDelegateHandle PresentationHandle;
	FDelegateHandle FixtureHandle;
	TArray<FGolemGearQuickUseRow> QuickUses;
	FText Objective;
	FText GearTitle;
	FTimerHandle ReadinessTimer;
	int32 Attempts = 0;
};
