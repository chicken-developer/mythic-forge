#include "Widgets/KnightRPGActionButton.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "CommonUITypes.h"
#include "Engine/DataTable.h"
#include "InputCoreTypes.h"

namespace KnightRPGButtonInput
{
	struct FKnightAcceptActionData : public FCommonInputActionDataBase
	{
		FKnightAcceptActionData()
		{
			DisplayName = NSLOCTEXT("KnightRPGUI", "FocusedAccept", "Select");
			DefaultGamepadInputTypeInfo.SetKey(EKeys::Gamepad_FaceButton_Bottom);
		}
	};
}

UKnightRPGActionButton::UKnightRPGActionButton()
{
	SetIsFocusable(true);
	SetMinDimensions(0, 36);
}

void UKnightRPGActionButton::SetFocusIdentity(const FName Identity)
{
	FocusIdentity = Identity;
}

FName UKnightRPGActionButton::GetFocusIdentity() const
{
	return FocusIdentity;
}

bool UKnightRPGActionButton::Initialize()
{
	if (AcceptInputTable == nullptr)
	{
		AcceptInputTable = NewObject<UDataTable>(this, TEXT("FocusedAcceptInput"), RF_Transient);
		AcceptInputTable->RowStruct = FCommonInputActionDataBase::StaticStruct();
		AcceptInputTable->AddRow(TEXT("Accept"), KnightRPGButtonInput::FKnightAcceptActionData());
	}
	// CommonButtonBase wraps the existing native content during Initialize, before Slate rebuilds it.
	if (WidgetTree == nullptr)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (WidgetTree->RootWidget == nullptr)
	{
		Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ButtonBackground"));
		Background->SetPadding(FMargin(12.0f, 7.0f));
		Caption = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ButtonCaption"));
		Caption->SetText(CaptionText);
		Caption->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Caption->SetAutoWrapText(true);
		Caption->SetJustification(ETextJustify::Center);
		Background->SetContent(Caption);
		WidgetTree->RootWidget = Background;
		RefreshStyle();
	}
	return Super::Initialize();
}

void UKnightRPGActionButton::SetNodeShape(uint8 InShape)
{
	Shape = InShape;
	bIsTalentNode = true;
	RefreshStyle();
}

void UKnightRPGActionButton::SetPresentationState(bool bInSelected, bool bInDraft)
{
	bSelected = bInSelected;
	bDraft = bInDraft;
	RefreshStyle();
}

void UKnightRPGActionButton::SetCompact(bool bInCompact)
{
	bCompact = bInCompact;
	SetMinDimensions(0, bCompact ? 22 : 36);
	RefreshStyle();
}

void UKnightRPGActionButton::RefreshStyle()
{
	if (Background == nullptr)
	{
		return;
	}
	const FLinearColor Outline = (bHovered || bHasPresentationFocus) ? FLinearColor::White :
		(bDraft ? FLinearColor(0.1f, 0.9f, 0.85f) :
		(bSelected ? FLinearColor(0.85f, 0.65f, 0.3f) : FLinearColor(0.26f, 0.35f, 0.38f)));
	const FLinearColor Fill = bSelected ? FLinearColor(0.23f, 0.19f, 0.1f) :
		FLinearColor(0.035f, 0.08f, 0.1f);
	Background->SetBrush(FSlateRoundedBoxBrush(Fill, Shape == 1 ? 48.0f : 3.0f, Outline, 2.0f));
	Background->SetPadding(bCompact ? FMargin(6.0f, 2.0f) : FMargin(12.0f, 7.0f));
	Background->SetBrushColor(FLinearColor::White);
	Background->SetRenderTransformAngle(Shape == 2 ? 45.0f : 0.0f);
	if (Caption != nullptr)
	{
		Caption->SetRenderTransformAngle(Shape == 2 ? -45.0f : 0.0f);
		FSlateFontInfo Font = Caption->GetFont();
		const int32 BaseSize = CaptionFontSize > 0 ? CaptionFontSize : (bIsTalentNode ? 11 : (bCompact ? 14 : 16));
		Font.Size = FMath::RoundToInt(BaseSize * AccessibilityScale);
		Caption->SetFont(Font);
		Caption->SetColorAndOpacity(FSlateColor(CaptionColor));
		Caption->SetJustification(bCompact && !bIsTalentNode ? ETextJustify::Left : ETextJustify::Center);
	}
}

void UKnightRPGActionButton::SetCaptionFontSize(int32 InFontSize)
{
	CaptionFontSize = FMath::Clamp(InFontSize, 0, 36);
	RefreshStyle();
}

void UKnightRPGActionButton::SetAccessibilityScale(const float Scale)
{
	AccessibilityScale = FMath::Clamp(Scale, 0.75f, 1.5f);
	SetMinDimensions(0, FMath::RoundToInt((bCompact ? 22.0f : 36.0f) * AccessibilityScale));
	RefreshStyle();
}

void UKnightRPGActionButton::SetCaptionColor(const FLinearColor& InColor)
{
	CaptionColor = InColor;
	RefreshStyle();
}

const FText& UKnightRPGActionButton::GetCaption() const
{
	return CaptionText;
}

void UKnightRPGActionButton::SetCaption(const FText& InCaption)
{
	CaptionText = InCaption;
	if (Caption != nullptr)
	{
		Caption->SetText(CaptionText);
	}
}

void UKnightRPGActionButton::NativeOnHovered()
{
	Super::NativeOnHovered();
	bHovered = true;
	RefreshStyle();
	OnInspectRequested.Broadcast();
}

void UKnightRPGActionButton::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	bHovered = false;
	RefreshStyle();
}

void UKnightRPGActionButton::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	if (AcceptInputTable != nullptr)
	{
		FDataTableRowHandle Accept;
		Accept.DataTable = AcceptInputTable;
		Accept.RowName = TEXT("Accept");
		// CommonUI routes virtual accept to the focused button before its mouse simulation.
		SetTriggeringInputAction(Accept);
	}
	bHasPresentationFocus = true;
	RefreshStyle();
	OnInspectRequested.Broadcast();
}

void UKnightRPGActionButton::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	SetTriggeringInputAction(FDataTableRowHandle());
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	bHasPresentationFocus = false;
	RefreshStyle();
}

void UKnightRPGActionButton::NativeDestruct()
{
	SetTriggeringInputAction(FDataTableRowHandle());
	Super::NativeDestruct();
}

FReply UKnightRPGActionButton::NativeOnMouseButtonDown(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && OnRefundRequested.IsBound())
	{
		OnRefundRequested.Broadcast();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
