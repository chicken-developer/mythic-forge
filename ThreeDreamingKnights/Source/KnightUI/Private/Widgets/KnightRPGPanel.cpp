#include "Widgets/KnightRPGPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "CommonInputModeTypes.h"
#include "Input/UIActionBindingHandle.h"
#include "InputCoreTypes.h"
#include "Widgets/KnightRPGActionButton.h"

UKnightRPGPanel::UKnightRPGPanel()
{
	bIsBackHandler = false;
	bIsModal = true;
	bSupportsActivationFocus = true;
	bAutoActivate = false;
}

void UKnightRPGPanel::SetSnapshot(const FKnightRPGPanelSnapshot& InSnapshot)
{
	Snapshot = InSnapshot;
	RefreshPresentation();
}

const FKnightRPGPanelSnapshot& UKnightRPGPanel::GetSnapshot() const
{
	return Snapshot;
}

TOptional<FUIInputConfig> UKnightRPGPanel::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture, false);
}

TSharedRef<SWidget> UKnightRPGPanel::RebuildWidget()
{
	if (WidgetTree == nullptr)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (WidgetTree->RootWidget == nullptr)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Canvas;
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(740.0f);
		Size->SetHeightOverride(720.0f);
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Size);
		CanvasSlot->SetAnchors(FAnchors(0.5f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetPosition(FVector2D::ZeroVector);
		CanvasSlot->SetAutoSize(true);
		UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
		Border->SetPadding(FMargin(20.0f));
		Border->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.04f, 0.98f));
		Size->SetContent(Border);
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
		Border->SetContent(Scroll);
		UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>();
		Scroll->AddChild(Body);
		for (int32 Index = 0; Index < 7; ++Index)
		{
			Lines.Add(AddLine(Body, FName(*FString::Printf(TEXT("SnapshotLine%d"), Index))));
		}
		AddAction(Body, EKnightRPGPanelAction::EquipWeapon, NSLOCTEXT("KnightRPGUI", "EquipWeapon", "1: Equip available weapon"));
		AddAction(Body, EKnightRPGPanelAction::UnequipWeapon, NSLOCTEXT("KnightRPGUI", "UnequipWeapon", "Unequip weapon"));
		AddAction(Body, EKnightRPGPanelAction::EquipRune, NSLOCTEXT("KnightRPGUI", "EquipRune", "2: Equip available rune"));
		AddAction(Body, EKnightRPGPanelAction::UnequipRune, NSLOCTEXT("KnightRPGUI", "UnequipRune", "Unequip rune"));
		AddAction(Body, EKnightRPGPanelAction::PurchaseHammerSkill,
			NSLOCTEXT("KnightRPGUI", "HammerSkill", "3: Buy Hammer Force"));
		AddAction(Body, EKnightRPGPanelAction::PurchaseRuneSkill,
			NSLOCTEXT("KnightRPGUI", "RuneSkill", "4: Buy Rune Focus"));
		AddAction(Body, EKnightRPGPanelAction::Respec, NSLOCTEXT("KnightRPGUI", "Respec", "5: Respec skills and refund paid costs"));
		AddAction(Body, EKnightRPGPanelAction::Continue, NSLOCTEXT("KnightRPGUI", "Continue", "Enter: Host Continue to encounter B"));
		AddAction(Body, EKnightRPGPanelAction::Close, NSLOCTEXT("KnightRPGUI", "Close", "Close (I / Escape / Menu)"));
	}
	RefreshPresentation();
	return Super::RebuildWidget();
}

UTextBlock* UKnightRPGPanel::AddLine(UVerticalBox* Parent, FName Name)
{
	UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Line->SetAutoWrapText(true);
	Line->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Parent->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.0f, 2.0f));
	return Line;
}

void UKnightRPGPanel::AddAction(UVerticalBox* Parent, EKnightRPGPanelAction Action, const FText& Caption)
{
	UKnightRPGActionButton* Button = WidgetTree->ConstructWidget<UKnightRPGActionButton>();
	Button->SetCaption(Caption);
	Button->OnClicked().AddWeakLambda(this, [this, Action]()
	{
		OnActionRequested.Broadcast(Action);
	});
	Parent->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 3.0f));
	Buttons.Add(Button);
}

void UKnightRPGPanel::RefreshPresentation()
{
	if (Lines.Num() != 7)
	{
		return;
	}
	const FText Values[] = {Snapshot.Title, Snapshot.Objective, Snapshot.Inventory, Snapshot.Equipment,
		Snapshot.Skills, Snapshot.Stats, Snapshot.Feedback};
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		Lines[Index]->SetText(Values[Index]);
	}
	if (Buttons.Num() == 9)
	{
		Buttons[7]->SetIsEnabled(Snapshot.bCanContinue);
	}
}

UWidget* UKnightRPGPanel::NativeGetDesiredFocusTarget() const
{
	return Buttons.IsEmpty() ? nullptr : Buttons[0].Get();
}

FReply UKnightRPGPanel::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::I
		|| InKeyEvent.GetKey() == EKeys::Gamepad_Special_Right
		|| InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		OnActionRequested.Broadcast(EKnightRPGPanelAction::Close);
		return FReply::Handled();
	}
	const FKey Key = InKeyEvent.GetKey();
	TOptional<EKnightRPGPanelAction> Action;
	if (Key == EKeys::One)
	{
		Action = EKnightRPGPanelAction::EquipWeapon;
	}
	else if (Key == EKeys::Two)
	{
		Action = EKnightRPGPanelAction::EquipRune;
	}
	else if (Key == EKeys::Three)
	{
		Action = EKnightRPGPanelAction::PurchaseHammerSkill;
	}
	else if (Key == EKeys::Four)
	{
		Action = EKnightRPGPanelAction::PurchaseRuneSkill;
	}
	else if (Key == EKeys::Five)
	{
		Action = EKnightRPGPanelAction::Respec;
	}
	else if (Key == EKeys::Enter)
	{
		// Preview precedes the focused CommonButton's Accept key, preventing an unrelated equip click.
		if (!Snapshot.bCanContinue)
		{
			return FReply::Handled();
		}
		Action = EKnightRPGPanelAction::Continue;
	}
	if (Action.IsSet())
	{
		OnActionRequested.Broadcast(Action.GetValue());
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
