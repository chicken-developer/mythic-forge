#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "KnightRPGActionButton.generated.h"

class UBorder;
class UDataTable;
class UTextBlock;

UCLASS()
class KNIGHTUI_API UKnightRPGActionButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UKnightRPGActionButton();
	virtual bool Initialize() override;
	void SetCaption(const FText& InCaption);
	const FText& GetCaption() const;
	void SetNodeShape(uint8 InShape);
	void SetPresentationState(bool bInSelected, bool bInDraft);
	void SetCompact(bool bInCompact);
	void SetCaptionFontSize(int32 InFontSize);
	void SetCaptionColor(const FLinearColor& InColor);
	FSimpleMulticastDelegate OnInspectRequested;
	FSimpleMulticastDelegate OnRefundRequested;

protected:
	virtual void NativeDestruct() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Caption;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> Background;
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> AcceptInputTable;
	FText CaptionText;
	uint8 Shape = 0;
	int32 CaptionFontSize = 0;
	FLinearColor CaptionColor = FLinearColor::White;
	bool bSelected = false;
	bool bDraft = false;
	bool bCompact = false;
	bool bHovered = false;
	bool bHasPresentationFocus = false;
	bool bIsTalentNode = false;
	void RefreshStyle();
};
