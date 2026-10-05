#include "Widgets/KnightRPGActionButton.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"

UKnightRPGActionButton::UKnightRPGActionButton()
{
	SetIsFocusable(true);
	SetMinDimensions(300, 36);
}

bool UKnightRPGActionButton::Initialize()
{
	// CommonButtonBase wraps the existing native content during Initialize, before Slate rebuilds it.
	if (WidgetTree == nullptr)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (WidgetTree->RootWidget == nullptr)
	{
		Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ButtonBackground"));
		Background->SetPadding(FMargin(12.0f, 7.0f));
		Background->SetBrushColor(FLinearColor(0.08f, 0.16f, 0.23f, 1.0f));
		Caption = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ButtonCaption"));
		Caption->SetText(CaptionText);
		Caption->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Background->SetContent(Caption);
		WidgetTree->RootWidget = Background;
	}
	return Super::Initialize();
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
	if (Background != nullptr)
	{
		Background->SetBrushColor(FLinearColor(0.13f, 0.34f, 0.48f, 1.0f));
	}
}

void UKnightRPGActionButton::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	if (Background != nullptr)
	{
		Background->SetBrushColor(FLinearColor(0.08f, 0.16f, 0.23f, 1.0f));
	}
}
