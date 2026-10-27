#include "Widgets/KnightRPGPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Settings/KnightGameUserSettings.h"
#include "Widgets/KnightRPGActionButton.h"

void UKnightRPGPanel::SetCampaignPresentation(const FKnightCampaignPanelSnapshot& InSnapshot)
{
	CampaignSnapshot = InSnapshot;
	if (CampaignNavigation == nullptr || !IsActivated())
	{
		return;
	}
	CampaignNavigation->SetVisibility(CampaignSnapshot.bEnabled ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	for (const TObjectPtr<UKnightRPGActionButton>& Button : TabButtons)
	{
		Button->SetIsEnabled(!CampaignSnapshot.bFrontEnd);
	}
	for (int32 Index = 0; Index < CampaignTabButtons.Num(); ++Index)
	{
		CampaignTabButtons[Index]->SetIsEnabled(!CampaignSnapshot.bFrontEnd || Index == 0 || Index == 3);
		CampaignTabButtons[Index]->SetPresentationState(
			static_cast<int32>(ActiveTab) == static_cast<int32>(EKnightRPGPanelTab::Campaign) + Index, false);
	}
	if (CampaignSnapshot.bFrontEnd && StoryBody != nullptr)
	{
		StoryBody->SetVisibility(ESlateVisibility::Collapsed);
	}
	else if (CampaignSnapshot.bEnabled && StoryText != nullptr)
	{
		StoryText->SetText(FText::Format(NSLOCTEXT("KnightCampaignUI", "ActiveObjective", "{0}\n{1}"),
			CampaignSnapshot.ActiveObjective, CampaignSnapshot.Subtitle));
	}
	if (CampaignSnapshot.bEnabled && ActiveTab >= EKnightRPGPanelTab::Campaign && CapturingBinding.IsNone())
	{
		RebuildActivePage();
	}
	ApplyCampaignTextScale();
	RefreshPresentation();
}

FReply UKnightRPGPanel::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!CapturingBinding.IsNone())
	{
		const FName ActionId = CapturingBinding;
		CapturingBinding = NAME_None;
		OnCampaignKeyRemapRequested.Broadcast(ActionId, InMouseEvent.GetEffectingButton());
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

const FKnightCampaignPanelSnapshot& UKnightRPGPanel::GetCampaignPresentation() const
{
	return CampaignSnapshot;
}

void UKnightRPGPanel::UpdateMapPosition(const FVector2D& UV, const bool bVisible)
{
	CampaignSnapshot.PlayerUV = UV;
	CampaignSnapshot.bPlayerVisible = bVisible;
	if (MapPlayerLabel != nullptr)
	{
		MapPlayerLabel->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* PositionSlot = Cast<UCanvasPanelSlot>(MapPlayerLabel->Slot))
		{
			PositionSlot->SetPosition(FVector2D(UV.X * 580.0, UV.Y * 320.0));
		}
	}
}

void UKnightRPGPanel::AddCampaignAction(UVerticalBox* Parent, const FText& Caption, const FName Action,
	const FName Context, const bool bEnabled)
{
	UKnightRPGActionButton* Button = MakeButton(Caption);
	Buttons.Add(Button);
	Button->SetFocusIdentity(FName(*(Action.ToString() + TEXT(":") + Context.ToString())));
	Button->SetIsEnabled(bEnabled && !CampaignSnapshot.bPending);
	Button->OnClicked().AddWeakLambda(this, [this, Action, Context]()
	{
		OnCampaignActionRequested.Broadcast(Action, Context);
	});
	Parent->AddChildToVerticalBox(Button);
}

void UKnightRPGPanel::BuildCampaign(UVerticalBox* Parent)
{
	if (!CampaignSnapshot.bEnabled)
	{
		return;
	}
	AddLine(Parent, CampaignSnapshot.SaveStatus, 16);
	AddLine(Parent, CampaignSnapshot.SessionStatus, 14);
	AddLine(Parent, CampaignSnapshot.Feedback, 15);
	if (!CampaignSnapshot.Confirmation.IsNone())
	{
		AddLine(Parent, CampaignSnapshot.Confirmation == TEXT("ResetSettings")
			? NSLOCTEXT("KnightCampaignUI", "ResetNotice", "Reset audio, controls and accessibility preferences to their defaults?")
			: (CampaignSnapshot.Confirmation == TEXT("Continue")
				? NSLOCTEXT("KnightCampaignUI", "LoadNotice", "Load the last checkpoint and discard unsaved progress?")
				: NSLOCTEXT("KnightCampaignUI", "ConfirmNotice", "New replaces the checkpoint at the next safe save. Quit discards progress after the last checkpoint.")), 16);
		AddCampaignAction(Parent, NSLOCTEXT("KnightCampaignUI", "Confirm", "Confirm"),
			TEXT("Confirm"), CampaignSnapshot.Confirmation);
		AddCampaignAction(Parent, NSLOCTEXT("KnightCampaignUI", "KeepPlaying", "Cancel / keep current progress"), TEXT("CancelConfirm"));
		return;
	}
	AddCampaignAction(Parent, NSLOCTEXT("KnightCampaignUI", "New", "New campaign"), TEXT("New"), NAME_None, CampaignSnapshot.bHost);
	AddCampaignAction(Parent, NSLOCTEXT("KnightCampaignUI", "Continue", "Continue last checkpoint"), TEXT("Continue"),
		NAME_None, CampaignSnapshot.bHost && CampaignSnapshot.bHasCheckpoint);
	AddCampaignAction(Parent, NSLOCTEXT("KnightCampaignUI", "Load", "Load / inspect checkpoint"), TEXT("InspectLoad"),
		NAME_None, CampaignSnapshot.bHost && CampaignSnapshot.bHasCheckpoint);
	if (!CampaignSnapshot.bFrontEnd)
	{
		AddCampaignAction(Parent, NSLOCTEXT("KnightCampaignUI", "Save", "Save safe checkpoint"), TEXT("Save"), NAME_None, CampaignSnapshot.bHost);
		AddCampaignAction(Parent, NSLOCTEXT("KnightCampaignUI", "Resume", "Resume game"), TEXT("Resume"));
		if (CampaignSnapshot.bHasDifficultyControls)
		{
			AddLine(Parent, CampaignSnapshot.Difficulty, 16);
			AddCampaignAction(Parent, NSLOCTEXT("KnightCampaignUI", "StoryDifficulty", "Difficulty: Story"),
				TEXT("Difficulty"), TEXT("Story"), CampaignSnapshot.bHost);
			AddCampaignAction(Parent, NSLOCTEXT("KnightCampaignUI", "StandardDifficulty", "Difficulty: Standard"),
				TEXT("Difficulty"), TEXT("Standard"), CampaignSnapshot.bHost);
		}
	}
	AddCampaignAction(Parent, NSLOCTEXT("KnightCampaignUI", "OpenSettings", "Settings"), TEXT("Settings"));
	AddCampaignAction(Parent, NSLOCTEXT("KnightCampaignUI", "Quit", "Quit game"), TEXT("Quit"));
	UKnightRPGActionButton* Online = MakeButton(NSLOCTEXT("KnightCampaignUI", "OnlineUnavailable", "Online play is unavailable in this build."));
	Online->SetIsEnabled(false);
	Parent->AddChildToVerticalBox(Online);
}

void UKnightRPGPanel::BuildJournal(UVerticalBox* Parent)
{
	AddLine(Parent, CampaignSnapshot.Tutorial, 16);
	AddLine(Parent, CampaignSnapshot.Subtitle, 18);
	for (const FKnightCampaignJournalRow& Quest : CampaignSnapshot.Quests)
	{
		AddLine(Parent, Quest.Title, 19);
		AddLine(Parent, Quest.Progress, 15);
		if (Quest.bCanActivate)
		{
			AddCampaignAction(Parent, NSLOCTEXT("KnightCampaignUI", "AcceptQuest", "Accept this quest"),
				TEXT("Activate"), Quest.QuestId, CampaignSnapshot.bHost);
		}
	}
	AddLine(Parent, CampaignSnapshot.Feedback, 14);
	AddCampaignAction(Parent, NSLOCTEXT("KnightCampaignUI", "CloseJournal", "Back to game"), TEXT("Resume"));
}

void UKnightRPGPanel::BuildCampaignMap(UVerticalBox* Parent)
{
	AddLine(Parent, CampaignSnapshot.RegionLabel, 20);
	AddLine(Parent, NSLOCTEXT("KnightCampaignUI", "MapLegend",
		"L landmark | Q quest | C checkpoint | T travel | ! hazard/ping | YOU current Knight. Dark cells are unexplored."), 13);
	UHorizontalBox* Regions = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Regions);
	for (const FKnightCampaignRegionRow& Region : CampaignSnapshot.Regions)
	{
		UKnightRPGActionButton* Button = MakeButton(Region.Label);
		Buttons.Add(Button);
		Button->SetFocusIdentity(FName(*(TEXT("Region:") + Region.RegionId.ToString())));
		Button->SetIsEnabled(Region.bDiscovered);
		const FName Id = Region.RegionId;
		Button->OnClicked().AddWeakLambda(this, [this, Id]()
		{
			SelectedMapMarker = NAME_None;
			OnCampaignActionRequested.Broadcast(TEXT("Region"), Id);
		});
		Regions->AddChildToHorizontalBox(Button)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Columns);
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(610.0f);
	Size->SetHeightOverride(355.0f);
	Columns->AddChildToHorizontalBox(Size);
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Size->SetContent(Canvas);
	for (int32 Y = 0; Y < 7; ++Y)
	{
		for (int32 X = 0; X < 7; ++X)
		{
			bool bExplored = false;
			for (const FKnightCampaignMarkerRow& Marker : CampaignSnapshot.Markers)
			{
				bExplored |= Marker.bDiscovered && FMath::Abs(Marker.UV.X * 7.0 - X - 0.5) < 1.4
					&& FMath::Abs(Marker.UV.Y * 7.0 - Y - 0.5) < 1.4;
			}
			UBorder* Tile = WidgetTree->ConstructWidget<UBorder>();
			Tile->SetBrushColor(bExplored ? FLinearColor(0.11f, 0.19f, 0.21f) : FLinearColor(0.018f, 0.024f, 0.027f));
			UCanvasPanelSlot* TileSlot = Canvas->AddChildToCanvas(Tile);
			TileSlot->SetPosition(FVector2D(X * 84.0, Y * 46.0));
			TileSlot->SetSize(FVector2D(82.0, 44.0));
		}
	}
	UVerticalBox* Legend = WidgetTree->ConstructWidget<UVerticalBox>();
	Columns->AddChildToHorizontalBox(Legend)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	bool bSelectedTravel = false;
	bool bSelectedPing = false;
	for (const FKnightCampaignMarkerRow& Marker : CampaignSnapshot.Markers)
	{
		if (!Marker.bDiscovered)
		{
			continue;
		}
		UKnightRPGActionButton* Icon = MakeButton(Marker.Symbol);
		Buttons.Add(Icon);
		Icon->SetFocusIdentity(FName(*(TEXT("MapIcon:") + Marker.MarkerId.ToString())));
		Icon->SetCompact(true);
		Icon->SetToolTipText(Marker.Label);
		UCanvasPanelSlot* IconSlot = Canvas->AddChildToCanvas(Icon);
		IconSlot->SetPosition(FVector2D(Marker.UV.X * 580.0, Marker.UV.Y * 320.0));
		IconSlot->SetSize(FVector2D(32.0, 28.0));
		IconSlot->SetZOrder(2);
		UKnightRPGActionButton* Row = MakeButton(FText::Format(NSLOCTEXT("KnightCampaignUI", "MapRow", "{0}  {1}"), Marker.Symbol, Marker.Label));
		Buttons.Add(Row);
		Row->SetFocusIdentity(FName(*(TEXT("MapRow:") + Marker.MarkerId.ToString())));
		Row->SetCompact(true);
		Row->SetPresentationState(SelectedMapMarker == Marker.MarkerId, false);
		const FName Id = Marker.MarkerId;
		const auto Select = [this, Id]()
		{
			SelectedMapMarker = Id;
			RebuildActivePage();
		};
		Icon->OnClicked().AddWeakLambda(this, Select);
		Row->OnClicked().AddWeakLambda(this, Select);
		Legend->AddChildToVerticalBox(Row);
		if (Id == SelectedMapMarker)
		{
			bSelectedTravel = Marker.bCanTravel;
			bSelectedPing = Marker.bCanPing;
		}
	}
	MapPlayerLabel = WidgetTree->ConstructWidget<UTextBlock>();
	MapPlayerLabel->SetText(NSLOCTEXT("KnightCampaignUI", "You", "YOU"));
	UCanvasPanelSlot* PlayerSlot = Canvas->AddChildToCanvas(MapPlayerLabel);
	PlayerSlot->SetSize(FVector2D(50.0, 25.0));
	PlayerSlot->SetZOrder(3);
	UpdateMapPosition(CampaignSnapshot.PlayerUV, CampaignSnapshot.bPlayerVisible);
	AddCampaignAction(Legend, NSLOCTEXT("KnightCampaignUI", "Ping", "Ping selected marker (8s)"),
		TEXT("Ping"), SelectedMapMarker, bSelectedPing);
	AddCampaignAction(Legend, NSLOCTEXT("KnightCampaignUI", "Travel", "Fast travel to selected marker"),
		TEXT("FastTravel"), SelectedMapMarker, CampaignSnapshot.bHost && bSelectedTravel);
	AddLine(Parent, CampaignSnapshot.Feedback, 14);
	AddCampaignAction(Parent, NSLOCTEXT("KnightCampaignUI", "CloseMap", "Back to game"), TEXT("Resume"));
}

void UKnightRPGPanel::BuildSettings(UVerticalBox* Parent)
{
	UKnightGameUserSettings* Settings = UKnightGameUserSettings::Get();
	if (Settings == nullptr)
	{
		AddLine(Parent, NSLOCTEXT("KnightCampaignUI", "NoSettings", "Local settings owner is unavailable."));
		return;
	}
	AddLine(Parent, NSLOCTEXT("KnightCampaignUI", "SettingsSaved", "Changes apply and save locally. Campaign progress belongs to the host."), 14);
	const TPair<FName, FText> Options[]{
		{TEXT("Display"), NSLOCTEXT("KnightSettings", "Display", "Display resolution")},
		{TEXT("Quality"), NSLOCTEXT("KnightSettings", "Quality", "Quality preset")},
		{TEXT("VSync"), NSLOCTEXT("KnightSettings", "VSync", "VSync")},
		{TEXT("Master"), NSLOCTEXT("KnightSettings", "Master", "Master volume %")},
		{TEXT("Music"), NSLOCTEXT("KnightSettings", "Music", "Music volume %")},
		{TEXT("Effects"), NSLOCTEXT("KnightSettings", "Effects", "Effects volume %")},
		{TEXT("Voice"), NSLOCTEXT("KnightSettings", "Voice", "Voice volume %")},
		{TEXT("Sensitivity"), NSLOCTEXT("KnightSettings", "Sensitivity", "Look sensitivity %")},
		{TEXT("Invert"), NSLOCTEXT("KnightSettings", "Invert", "Invert vertical look")},
		{TEXT("ToggleBlock"), NSLOCTEXT("KnightSettings", "ToggleBlock", "Toggle block instead of hold")},
		{TEXT("HoldLock"), NSLOCTEXT("KnightSettings", "HoldLock", "Hold target lock instead of toggle")},
		{TEXT("Text"), NSLOCTEXT("KnightSettings", "Text", "UI text size %")},
		{TEXT("Subtitles"), NSLOCTEXT("KnightSettings", "Subtitles", "Subtitle size %")},
		{TEXT("SubtitleBackground"), NSLOCTEXT("KnightSettings", "SubtitleBG", "Subtitle background")},
		{TEXT("Shake"), NSLOCTEXT("KnightSettings", "Shake", "Camera shake %")},
		{TEXT("ReducedMotion"), NSLOCTEXT("KnightSettings", "ReducedMotion", "Reduced camera motion")}};
	for (const TPair<FName, FText>& Option : Options)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Parent->AddChildToVerticalBox(Row);
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		FSlateFontInfo LabelFont = Label->GetFont();
		LabelFont.Size = 16;
		Label->SetFont(LabelFont);
		Label->SetText(FText::Format(NSLOCTEXT("KnightSettings", "Row", "{0}: {1}"),
			Option.Value, Settings->GetPreferenceLabel(Option.Key)));
		Label->SetAutoWrapText(true);
		Row->AddChildToHorizontalBox(Label)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		for (const bool bIncrease : {false, true})
		{
			UKnightRPGActionButton* Button = MakeButton(bIncrease
				? NSLOCTEXT("KnightSettings", "Increase", "+ / toggle") : NSLOCTEXT("KnightSettings", "Decrease", "- / toggle"));
			Buttons.Add(Button);
			const FName Id = Option.Key;
			Button->SetFocusIdentity(FName(*((bIncrease ? FString(TEXT("SettingUp:")) : FString(TEXT("SettingDown:"))) + Id.ToString())));
			Button->OnClicked().AddWeakLambda(this, [this, Id, bIncrease]()
			{
				OnCampaignActionRequested.Broadcast(bIncrease ? TEXT("SettingUp") : TEXT("SettingDown"), Id);
			});
			Row->AddChildToHorizontalBox(Button);
		}
	}
	AddLine(Parent, NSLOCTEXT("KnightSettings", "RemapHelp", "Select an action, then press a keyboard/mouse key. Escape cancels. Conflicting/reserved keys are rejected."), 14);
	if (!CapturingBinding.IsNone())
	{
		AddLine(Parent, NSLOCTEXT("KnightSettings", "CaptureNow", "Press the replacement key now. Escape cancels."), 18);
	}
	for (const FKnightUserKeyBinding& Binding : Settings->KeyBindings)
	{
		UKnightRPGActionButton* Button = MakeButton(FText::Format(NSLOCTEXT("KnightSettings", "Binding", "{0}: [{1}]"),
			FText::FromName(Binding.ActionId), Binding.Key.GetDisplayName()));
		const FName Id = Binding.ActionId;
		Buttons.Add(Button);
		Button->SetFocusIdentity(FName(*(TEXT("Binding:") + Id.ToString())));
		Button->OnClicked().AddWeakLambda(this, [this, Id]()
		{
			CapturingBinding = Id;
			RebuildActivePage();
		});
		Parent->AddChildToVerticalBox(Button);
	}
	AddCampaignAction(Parent, NSLOCTEXT("KnightSettings", "Reset", "Reset audio, controls and accessibility (confirm)"), TEXT("ResetSettings"));
	AddLine(Parent, CampaignSnapshot.Feedback, 14);
	AddCampaignAction(Parent, NSLOCTEXT("KnightSettings", "Back", "Back"), TEXT("SettingsBack"));
}

void UKnightRPGPanel::ApplyCampaignTextScale()
{
	if (!CampaignSnapshot.bEnabled || WidgetTree == nullptr)
	{
		return;
	}
	const float Scale = FMath::Clamp(CampaignSnapshot.TextScale, 0.75f, 1.5f);
	WidgetTree->ForEachWidget([this, Scale](UWidget* Widget)
	{
		if (UKnightRPGActionButton* Button = Cast<UKnightRPGActionButton>(Widget))
		{
			Button->SetAccessibilityScale(Scale);
		}
		if (UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			const TWeakObjectPtr<UTextBlock> Key(Text);
			int32* Original = OriginalFontSizes.Find(Key);
			if (Original == nullptr)
			{
				OriginalFontSizes.Add(Key, Text->GetFont().Size);
				Original = OriginalFontSizes.Find(Key);
			}
			FSlateFontInfo Font = Text->GetFont();
			Font.Size = FMath::Max(10, FMath::RoundToInt(*Original * Scale));
			Text->SetFont(Font);
		}
	});
	for (auto It = OriginalFontSizes.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
