#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "GolemRPGActionButton.generated.h"

class UBorder;
class UDataTable;
class UTextBlock;

UCLASS()
class GOLEMUI_API UGolemRPGActionButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UGolemRPGActionButton();
	virtual bool Initialize() override;
	void SetCaption(const FText& InCaption);
	const FText& GetCaption() const;
	void SetNodeShape(uint8 InShape);
	void SetPresentationState(bool bInSelected, bool bInDraft);
	void SetCompact(bool bInCompact);
	void SetCaptionFontSize(int32 InFontSize);
	void SetAccessibilityScale(float Scale);
	void SetFocusIdentity(FName Identity);
	FName GetFocusIdentity() const;
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
	FName FocusIdentity;
	uint8 Shape = 0;
	int32 CaptionFontSize = 0;
	FLinearColor CaptionColor = FLinearColor::White;
	bool bSelected = false;
	bool bDraft = false;
	bool bCompact = false;
	bool bHovered = false;
	bool bHasPresentationFocus = false;
	bool bIsTalentNode = false;
	float AccessibilityScale = 1.0f;
	void RefreshStyle();
};
