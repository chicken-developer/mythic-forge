#pragma once

#include "CoreMinimal.h"
#include "SkillLab/KnightSkillLabHUD.h"
#include "Models/KnightGearPanelTypes.h"
#include "KnightGearLabHUD.generated.h"

class AKnightGearLabState;
class AKnightRPGUIPlayerController;

/** Event-bound quick-use and fixture presentation layered on the M5.5 HUD. */
UCLASS()
class THREEDREAMINGKNIGHTS_API AKnightGearLabHUD : public AKnightSkillLabHUD
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
	TWeakObjectPtr<AKnightRPGUIPlayerController> GearController;
	TWeakObjectPtr<AKnightGearLabState> GearState;
	FDelegateHandle PresentationHandle;
	FDelegateHandle FixtureHandle;
	TArray<FKnightGearQuickUseRow> QuickUses;
	FText Objective;
	FText GearTitle;
	FTimerHandle ReadinessTimer;
	int32 Attempts = 0;
};
