#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Models/KnightRPGPanelTypes.h"
#include "KnightRPGPanel.generated.h"

class UKnightRPGActionButton;
class UTextBlock;
class UVerticalBox;

/** A real CommonUI input layer with native content; actions are intents consumed by the player composition. */
UCLASS()
class KNIGHTUI_API UKnightRPGPanel : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UKnightRPGPanel();
	void SetSnapshot(const FKnightRPGPanelSnapshot& InSnapshot);
	const FKnightRPGPanelSnapshot& GetSnapshot() const;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	FOnKnightRPGPanelAction OnActionRequested;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UTextBlock* AddLine(UVerticalBox* Parent, FName Name);
	void AddAction(UVerticalBox* Parent, EKnightRPGPanelAction Action, const FText& Caption);
	void RefreshPresentation();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> Lines;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UKnightRPGActionButton>> Buttons;
	FKnightRPGPanelSnapshot Snapshot;
};
