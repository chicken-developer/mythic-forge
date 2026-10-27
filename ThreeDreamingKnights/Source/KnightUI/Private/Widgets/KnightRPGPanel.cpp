#include "Widgets/KnightRPGPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PointLightComponent.h"
#include "Components/ScaleBox.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "CommonInputModeTypes.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "InputCoreTypes.h"
#include "Math/RotationMatrix.h"
#include "Widgets/KnightRPGActionButton.h"

namespace KnightRPGPresentation
{
	UVerticalBox* AddColumn(UWidgetTree* Tree, UHorizontalBox* Parent, float Fill = 1.0f)
	{
		UBorder* Frame = Tree->ConstructWidget<UBorder>();
		Frame->SetBrushColor(FLinearColor(0.035f, 0.045f, 0.05f, 1.0f));
		Frame->SetPadding(FMargin(14.0f));
		UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Frame);
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = Fill;
		Slot->SetSize(Size);
		Slot->SetPadding(FMargin(4.0f));
		UVerticalBox* Column = Tree->ConstructWidget<UVerticalBox>();
		Frame->SetContent(Column);
		return Column;
	}

	FText ChoiceLabel(FName Choice)
	{
		return Choice.IsNone() ? NSLOCTEXT("KnightRPGUI", "NoChoice", "0/1") :
			NSLOCTEXT("KnightRPGUI", "SelectedChoice", "1/1");
	}

	void AddChoiceConnector(UWidgetTree* Tree, UCanvasPanel* Canvas, const FVector2D& Start, const FVector2D& End)
	{
		UBorder* Line = Tree->ConstructWidget<UBorder>();
		Line->SetBrushColor(FLinearColor(0.25f, 0.65f, 0.75f, 1.0f));
		Line->SetPadding(FMargin(0.0f));
		Line->SetVisibility(ESlateVisibility::HitTestInvisible);
		Line->SetRenderTransformPivot(FVector2D(0.0f, 0.5f));
		const FVector2D Delta = End - Start;
		Line->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
		UCanvasPanelSlot* LineSlot = Canvas->AddChildToCanvas(Line);
		LineSlot->SetPosition(Start);
		LineSlot->SetSize(FVector2D(Delta.Size(), 2.0f));
		LineSlot->SetZOrder(-1);
	}
}

UKnightRPGPanel::UKnightRPGPanel()
{
	// This project binds native Escape/gamepad Back and has no CommonUI default back-action asset.
	bIsBackHandler = false;
	bIsModal = true;
	bSupportsActivationFocus = true;
	bAutoActivate = false;
}

void UKnightRPGPanel::SetSnapshot(const FKnightRPGPanelSnapshot& InSnapshot)
{
	const bool bIdentityChanged = !(Snapshot.KnightId == InSnapshot.KnightId);
	const bool bRevisionChanged = Snapshot.Revision != InSnapshot.Revision;
	const bool bGearIdentityChanged = !(Snapshot.Gear.KnightId == InSnapshot.Gear.KnightId);
	const bool bGearChanged = Snapshot.Gear.InventoryRevision != InSnapshot.Gear.InventoryRevision ||
		Snapshot.Gear.KnightRevision != InSnapshot.Gear.KnightRevision || bGearIdentityChanged ||
		!Snapshot.Gear.RelicStatus.EqualTo(InSnapshot.Gear.RelicStatus) ||
		Snapshot.Gear.bEnabled != InSnapshot.Gear.bEnabled || Snapshot.Gear.bCanEdit != InSnapshot.Gear.bCanEdit ||
		Snapshot.Gear.bCatalogLab != InSnapshot.Gear.bCatalogLab ||
		Snapshot.Gear.ClaimableRewardId != InSnapshot.Gear.ClaimableRewardId ||
		!Snapshot.Gear.FixtureSummary.EqualTo(InSnapshot.Gear.FixtureSummary);
	bool bGearStatsChanged = Snapshot.Gear.bEnabled && Snapshot.StatRows.Num() != InSnapshot.StatRows.Num();
	if (Snapshot.Gear.bEnabled && !bGearStatsChanged)
	{
		for (int32 Index = 0; Index < Snapshot.StatRows.Num(); ++Index)
		{
			bGearStatsChanged |= !Snapshot.StatRows[Index].Value.EqualTo(InSnapshot.StatRows[Index].Value) ||
				!Snapshot.StatRows[Index].Contributions.EqualTo(InSnapshot.StatRows[Index].Contributions);
		}
	}
	const bool bLockChanged = Snapshot.bCanEditBuild != InSnapshot.bCanEditBuild ||
		Snapshot.bCommandPending != InSnapshot.bCommandPending;
	const bool bCompleted = PendingRequestId.IsValid() &&
		PendingRequestId == InSnapshot.LastCompletedRequestId;
	Snapshot = InSnapshot;
	if (bGearIdentityChanged || bCompleted)
	{
		ResetGearDraft();
	}
	if (bIdentityChanged || bCompleted || (!bHasDraft && bRevisionChanged))
	{
		Draft = Snapshot.Build;
		DraftRevision = Snapshot.Revision;
		DraftWeaponRuneInstanceId = Snapshot.WeaponRuneInstanceId;
		DraftSkillRuneInstanceId = Snapshot.SkillRuneInstanceId;
		bHasDraft = false;
		SelectedLoadoutId = Snapshot.ActiveTalentLoadoutId;
		Preview = FKnightRPGBuildPreview();
		Preview.KnightId = Snapshot.KnightId;
		Preview.Build = Draft;
		Preview.Spells = Snapshot.Spellbook;
		Preview.Runes = Snapshot.Runes;
	}
	if (bCompleted || bIdentityChanged)
	{
		PendingRequestId.Invalidate();
		bPending = false;
	}
	bPending = bPending || Snapshot.bCommandPending;
	if (bIdentityChanged || bRevisionChanged || bCompleted || bLockChanged || bGearChanged || bGearStatsChanged)
	{
		RebuildActivePage();
	}
	RefreshPresentation();
}

void UKnightRPGPanel::SetBuildPreview(const FKnightRPGBuildPreview& InPreview)
{
	if (InPreview.KnightId == Snapshot.KnightId && InPreview.Build == Draft)
	{
		Preview = InPreview;
	}
}

void UKnightRPGPanel::SetActiveTab(EKnightRPGPanelTab InTab)
{
	if (ActiveTab != InTab)
	{
		CapturingBinding = NAME_None;
	}
	ActiveTab = InTab;
	ShowDetail(NSLOCTEXT("KnightRPGUI", "InspectTabHelp", "Hover / focus to inspect. Apply staged changes at sanctuary."));
	if (PageBody != nullptr)
	{
		if (UScrollBox* Scroll = Cast<UScrollBox>(PageBody->GetParent()))
		{
			Scroll->ScrollToStart();
		}
	}
	RebuildActivePage();
	RefreshPresentation();
}

EKnightRPGPanelTab UKnightRPGPanel::GetActiveTab() const
{
	return ActiveTab;
}

const FKnightRPGPanelSnapshot& UKnightRPGPanel::GetSnapshot() const
{
	return Snapshot;
}

const FKnightSkillBuild& UKnightRPGPanel::GetDraftBuild() const
{
	return Draft;
}

bool UKnightRPGPanel::HasPendingCommand() const
{
	return bPending;
}

FGuid UKnightRPGPanel::GetPendingRequestId() const
{
	return PendingRequestId;
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
		UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
		Scale->SetStretch(EStretch::ScaleToFit);
		WidgetTree->RootWidget = Scale;
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(1320.0f);
		Size->SetHeightOverride(760.0f);
		Scale->SetContent(Size);
		UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
		Border->SetPadding(FMargin(18.0f));
		Border->SetBrushColor(FLinearColor(0.016f, 0.021f, 0.025f, 0.99f));
		Size->SetContent(Border);
		UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>();
		Border->SetContent(Body);
		HeaderText = AddLine(Body, FText(), 25);
		ObjectiveText = AddLine(Body, FText(), 14);
		StoryBody = WidgetTree->ConstructWidget<UVerticalBox>();
		Body->AddChildToVerticalBox(StoryBody);
		StoryBody->SetVisibility(ESlateVisibility::Collapsed);
		StoryText = AddLine(StoryBody, FText(), 14);
		StoryChoiceBody = WidgetTree->ConstructWidget<UHorizontalBox>();
		StoryBody->AddChildToVerticalBox(StoryChoiceBody);
		UHorizontalBox* StoryActions = WidgetTree->ConstructWidget<UHorizontalBox>();
		StoryBody->AddChildToVerticalBox(StoryActions);
		const FName StoryOperations[]{TEXT("Advance"), TEXT("Cancel"), TEXT("Skip"), TEXT("Save"), TEXT("Continue"), TEXT("Activate")};
		const FText StoryCaptions[]{
			NSLOCTEXT("KnightStoryUI", "Advance", "Next"),
			NSLOCTEXT("KnightStoryUI", "Cancel", "Cancel"),
			NSLOCTEXT("KnightStoryUI", "Skip", "Skip"),
			NSLOCTEXT("KnightStoryUI", "Save", "Save checkpoint"),
			NSLOCTEXT("KnightStoryUI", "Load", "Continue"),
			NSLOCTEXT("KnightStoryUI", "Accept", "Accept quest")};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(StoryOperations); ++Index)
		{
			UKnightRPGActionButton* StoryButton = MakeButton(StoryCaptions[Index]);
			StoryButton->SetCompact(true);
			const FName Operation = StoryOperations[Index];
			StoryButton->OnClicked().AddWeakLambda(this, [this, Operation]()
			{
				OnStoryActionRequested.Broadcast(Operation);
			});
			StoryActions->AddChildToHorizontalBox(StoryButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			StoryButtons.Add(StoryButton);
		}
		UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
		CampaignNavigation = WidgetTree->ConstructWidget<UHorizontalBox>();
		CampaignNavigation->SetVisibility(ESlateVisibility::Collapsed);
		Body->AddChildToVerticalBox(CampaignNavigation)->SetPadding(FMargin(0.0f, 4.0f));
		const FText CampaignCaptions[]{
			NSLOCTEXT("KnightCampaignUI", "Campaign", "Campaign / Pause"),
			NSLOCTEXT("KnightCampaignUI", "Journal", "Quest journal"),
			NSLOCTEXT("KnightCampaignUI", "Map", "World map"),
			NSLOCTEXT("KnightCampaignUI", "Settings", "Settings")};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(CampaignCaptions); ++Index)
		{
			UKnightRPGActionButton* Button = MakeButton(CampaignCaptions[Index]);
			Button->SetCompact(true);
			Button->OnClicked().AddWeakLambda(this, [this, Index]()
			{
				SetActiveTab(static_cast<EKnightRPGPanelTab>(static_cast<int32>(EKnightRPGPanelTab::Campaign) + Index));
			});
			CampaignNavigation->AddChildToHorizontalBox(Button)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			CampaignTabButtons.Add(Button);
		}
		Body->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.0f, 8.0f));
		const FText Captions[] =
		{
			NSLOCTEXT("KnightRPGUI", "Character", "Character"),
			NSLOCTEXT("KnightRPGUI", "Specialization", "Specialization"),
			NSLOCTEXT("KnightRPGUI", "Talents", "Talents"),
			NSLOCTEXT("KnightRPGUI", "Spellbook", "Spellbook"),
			NSLOCTEXT("KnightRPGUI", "Runes", "Runes"),
			NSLOCTEXT("KnightRPGUI", "Inventory", "Inventory")
		};
		const int32 TabCount = Snapshot.Gear.bEnabled ? UE_ARRAY_COUNT(Captions) : UE_ARRAY_COUNT(Captions) - 1;
		for (int32 Index = 0; Index < TabCount; ++Index)
		{
			UKnightRPGActionButton* Button = MakeButton(Captions[Index]);
			Button->OnClicked().AddWeakLambda(this, [this, Index]()
			{
				SetActiveTab(static_cast<EKnightRPGPanelTab>(Index));
			});
			Tabs->AddChildToHorizontalBox(Button)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			TabButtons.Add(Button);
		}
		USizeBox* PageSize = WidgetTree->ConstructWidget<USizeBox>();
		Body->AddChildToVerticalBox(PageSize)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UScrollBox* PageScroll = WidgetTree->ConstructWidget<UScrollBox>();
		PageScroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
		PageSize->SetContent(PageScroll);
		PageBody = WidgetTree->ConstructWidget<UVerticalBox>();
		PageScroll->AddChild(PageBody);
		CampaignDetailHelp = AddLine(Body, NSLOCTEXT("KnightRPGUI", "DetailScrollHelp", "Details  [PgUp / PgDn, LT / RT to scroll]"), 11);
		USizeBox* DetailSize = WidgetTree->ConstructWidget<USizeBox>();
		CampaignDetailContainer = DetailSize;
		DetailSize->SetHeightOverride(64.0f);
		Body->AddChildToVerticalBox(DetailSize);
		DetailScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("RPGDetailsScroll"));
		DetailScroll->SetAllowOverscroll(false);
		DetailSize->SetContent(DetailScroll);
		UVerticalBox* DetailBody = WidgetTree->ConstructWidget<UVerticalBox>();
		DetailScroll->AddChild(DetailBody);
		DetailText = AddLine(DetailBody,
			NSLOCTEXT("KnightRPGUI", "InspectHint", "Hover / focus to inspect. Diamond = choice; circle = passive; square = active."), 14);
		DetailText->SetMinDesiredWidth(1100.0f);
		StatusText = AddLine(Body, FText(), 14);
		UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
		PanelFooter = Footer;
		Body->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0.0f, 6.0f));
		UKnightRPGActionButton* Revert = MakeButton(NSLOCTEXT("KnightRPGUI", "Revert", "Revert / Reset draft"));
		Revert->OnClicked().AddWeakLambda(this, [this]()
		{
			RevertDraft();
		});
		Footer->AddChildToHorizontalBox(Revert)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ApplyButton = MakeButton(NSLOCTEXT("KnightRPGUI", "Apply", "Apply changes [F]"));
		ApplyButton->OnClicked().AddWeakLambda(this, [this]()
		{
			if (bGearDraft)
			{
				SubmitGearCommand();
			}
			else
			{
				SubmitCommand(EKnightRPGPanelAction::ApplyBuild);
			}
		});
		Footer->AddChildToHorizontalBox(ApplyButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ContinueButton = MakeButton(NSLOCTEXT("KnightRPGUI", "Continue", "Host Continue"));
		ContinueButton->OnClicked().AddWeakLambda(this, [this]()
		{
			if (Snapshot.bCanContinue && !bPending)
			{
				OnActionRequested.Broadcast(EKnightRPGPanelAction::Continue);
			}
		});
		Footer->AddChildToHorizontalBox(ContinueButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UKnightRPGActionButton* Close = MakeButton(NSLOCTEXT("KnightRPGUI", "Close", "Close [Esc / B]"));
		Close->OnClicked().AddWeakLambda(this, [this]()
		{
			OnActionRequested.Broadcast(EKnightRPGPanelAction::Close);
		});
		Footer->AddChildToHorizontalBox(Close)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	RebuildActivePage();
	RefreshPresentation();
	if (CampaignSnapshot.bEnabled)
	{
		SetCampaignPresentation(CampaignSnapshot);
	}
	return Super::RebuildWidget();
}

void UKnightRPGPanel::SetStoryPresentation(const FText& Text, const bool bHost, const bool bCommandPending)
{
	if (StoryBody != nullptr)
	{
		StoryBody->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		ObjectiveText->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		ContinueButton->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		StoryText->SetText(Text);
		for (UKnightRPGActionButton* Button : StoryButtons)
		{
			Button->SetIsEnabled(bHost && !bCommandPending);
		}
	}
}

void UKnightRPGPanel::SetStoryChoices(const TArray<FKnightStoryChoicePresentation>& Choices, const bool bCommandPending)
{
	if (StoryChoiceBody == nullptr)
	{
		return;
	}
	bool bChanged = DisplayedStoryChoices.Num() != Choices.Num();
	for (int32 Index = 0; !bChanged && Index < Choices.Num(); ++Index)
	{
		bChanged = DisplayedStoryChoices[Index].AnchorId != Choices[Index].AnchorId
			|| DisplayedStoryChoices[Index].ChoiceId != Choices[Index].ChoiceId;
	}
	if (bChanged)
	{
		DisplayedStoryChoices = Choices;
		StoryChoiceBody->ClearChildren();
		for (const FKnightStoryChoicePresentation& Choice : Choices)
		{
			UKnightRPGActionButton* Button = MakeButton(Choice.Label);
			Button->OnClicked().AddWeakLambda(this, [this, Choice]()
			{
				OnStoryChoiceRequested.Broadcast(Choice.AnchorId, Choice.ChoiceId);
			});
			StoryChoiceBody->AddChildToHorizontalBox(Button)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
	for (UWidget* Child : StoryChoiceBody->GetAllChildren())
	{
		Child->SetIsEnabled(!bCommandPending);
	}
}

UTextBlock* UKnightRPGPanel::AddLine(UVerticalBox* Parent, const FText& Text, int32 FontSize)
{
	UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>();
	Line->SetAutoWrapText(true);
	Line->SetText(Text);
	Line->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.78f)));
	FSlateFontInfo Font = Line->GetFont();
	Font.Size = FontSize;
	Line->SetFont(Font);
	Parent->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.0f, 3.0f));
	return Line;
}

UKnightRPGActionButton* UKnightRPGPanel::MakeButton(const FText& Caption, const FText& Tooltip)
{
	UKnightRPGActionButton* Button = WidgetTree->ConstructWidget<UKnightRPGActionButton>();
	Button->SetCaption(Caption);
	Button->SetToolTipText(Tooltip);
	if (!Tooltip.IsEmpty())
	{
		Button->OnInspectRequested.AddWeakLambda(this, [this, Tooltip]()
		{
			ShowDetail(Tooltip);
		});
	}
	return Button;
}

void UKnightRPGPanel::AddAction(UVerticalBox* Parent, EKnightRPGPanelAction Action, const FText& Caption)
{
	const FText Comparison = Action == EKnightRPGPanelAction::EquipWeapon ? Snapshot.WeaponComparison :
		(Action == EKnightRPGPanelAction::EquipRune ? Snapshot.RuneComparison : FText());
	UKnightRPGActionButton* Button = MakeButton(Caption, Comparison);
	Button->SetIsEnabled(!bPending);
	Button->OnClicked().AddWeakLambda(this, [this, Action]()
	{
		if (!bPending)
		{
			OnActionRequested.Broadcast(Action);
		}
	});
	Parent->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 4.0f));
	Buttons.Add(Button);
}

void UKnightRPGPanel::RefreshPresentation()
{
	if (HeaderText == nullptr)
	{
		return;
	}
	HeaderText->SetText(Snapshot.Title);
	if (Snapshot.Gear.bEnabled && (ActiveTab == EKnightRPGPanelTab::Character || ActiveTab == EKnightRPGPanelTab::Inventory))
	{
		HeaderText->SetText(FText::Format(NSLOCTEXT("KnightGearUI", "Title", "{0} — Character & Gear"), Snapshot.Gear.KnightName));
	}
	ObjectiveText->SetText(Snapshot.Objective);
	FText State = NSLOCTEXT("KnightRPGUI", "Browse", "Browse • confirmed state");
	if (bHasDraft)
	{
		State = DraftRevision == Snapshot.Revision ?
			NSLOCTEXT("KnightRPGUI", "Draft", "Draft • cyan nodes are proposed, actor unchanged") :
			NSLOCTEXT("KnightRPGUI", "StaleDraft", "Draft is stale • Apply will be validated against the original revision; Revert to refresh");
	}
	if (bGearDraft)
	{
		State = GearDraft.ExpectedInventoryRevision == Snapshot.Gear.InventoryRevision &&
			GearDraft.ExpectedKnightRevision == Snapshot.Gear.KnightRevision
			? NSLOCTEXT("KnightGearUI", "Draft", "Gear draft: preview only. Apply confirms the exact item and revisions.")
			: NSLOCTEXT("KnightGearUI", "Stale", "Gear draft is stale. Apply validates its original revisions; Revert to refresh.");
	}
	if (bPending)
	{
		State = NSLOCTEXT("KnightRPGUI", "Pending", "Pending • waiting for server confirmation");
	}
	StatusText->SetText(FText::Format(NSLOCTEXT("KnightRPGUI", "Status", "{0}   |   {1}   {2}"),
		State, Snapshot.Feedback, Snapshot.bCanEditBuild ? FText() : Snapshot.EditLockReason));
	if (CampaignSnapshot.bEnabled && ActiveTab >= EKnightRPGPanelTab::Campaign)
	{
		HeaderText->SetText(NSLOCTEXT("KnightCampaignUI", "CampaignTitle", "Three Dreaming Knights"));
		StatusText->SetText(CampaignSnapshot.SaveStatus);
	}
	if (CampaignSnapshot.bEnabled)
	{
		ObjectiveText->SetText(CampaignSnapshot.bFrontEnd ? CampaignSnapshot.SessionStatus : CampaignSnapshot.ActiveObjective);
	}
	const bool bCampaignPage = CampaignSnapshot.bEnabled && ActiveTab >= EKnightRPGPanelTab::Campaign;
	if (CampaignDetailContainer != nullptr && CampaignDetailHelp != nullptr)
	{
		CampaignDetailContainer->SetVisibility(bCampaignPage ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		CampaignDetailHelp->SetVisibility(bCampaignPage ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (PanelFooter != nullptr)
	{
		PanelFooter->SetVisibility(CampaignSnapshot.bEnabled && ActiveTab >= EKnightRPGPanelTab::Campaign
			? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (ApplyButton != nullptr)
	{
		ApplyButton->SetIsEnabled(!bPending && ((bHasDraft && CanStage()) || (bGearDraft && Snapshot.Gear.bCanEdit)));
	}
	if (ContinueButton != nullptr)
	{
		ContinueButton->SetIsEnabled(Snapshot.bCanContinue && !bPending);
	}
	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		TabButtons[Index]->SetPresentationState(Index == static_cast<int32>(ActiveTab), false);
	}
	for (int32 Index = 0; Index < StatValueButtons.Num() && Index < Snapshot.StatRows.Num(); ++Index)
	{
		const FKnightRPGStatRow& Row = Snapshot.StatRows[Index];
		StatValueButtons[Index]->SetCaption(FText::Format(NSLOCTEXT("KnightRPGUI", "StatRow", "{0}:  {1}"),
			Row.Name, Row.Value));
		StatValueButtons[Index]->SetToolTipText(Row.Contributions);
	}
}

void UKnightRPGPanel::RebuildActivePage()
{
	if (PageBody == nullptr || bRebuildingPage)
	{
		return;
	}
	TGuardValue<bool> Guard(bRebuildingPage, true);
	CancelDeferredPageFocus();
	FText FocusedCaption;
	FName FocusedIdentity;
	const bool bRestoreFocus = IsActivated() && GetOwningPlayer() != nullptr;
	for (const TObjectPtr<UKnightRPGActionButton>& Button : Buttons)
	{
		if (bRestoreFocus && Button != nullptr &&
			(Button->HasUserFocus(GetOwningPlayer()) || Button->HasUserFocusedDescendants(GetOwningPlayer())))
		{
			FocusedCaption = Button->GetCaption();
			FocusedIdentity = Button->GetFocusIdentity();
			break;
		}
	}
	const FText NameEntry = LoadoutName != nullptr ? LoadoutName->GetText() : FText();
	UE_LOG(LogTemp, Verbose, TEXT("PageFocusCapture page=%d identity=%s caption=%s"),
		static_cast<int32>(ActiveTab), *FocusedIdentity.ToString(), *FocusedCaption.ToString());
	const bool bRestoreNameFocus = bRestoreFocus && LoadoutName != nullptr &&
		(LoadoutName->HasUserFocus(GetOwningPlayer()) || LoadoutName->HasUserFocusedDescendants(GetOwningPlayer()));
	PageBody->ClearChildren();
	Buttons.Reset();
	StatValueButtons.Reset();
	PaperDollImage = nullptr;
	LoadoutSelector = nullptr;
	LoadoutName = nullptr;
	ClassChoiceFirstButton = nullptr;
	SpecChoiceFirstButton = nullptr;
	GearTargetSelector = nullptr;
	GearTransferSelector = nullptr;
	MapPlayerLabel = nullptr;
	switch (ActiveTab)
	{
	case EKnightRPGPanelTab::Character: BuildCharacter(PageBody); break;
	case EKnightRPGPanelTab::Specialization: BuildSpecialization(PageBody); break;
	case EKnightRPGPanelTab::Talents: BuildTalents(PageBody); break;
	case EKnightRPGPanelTab::Spellbook: BuildSpellbook(PageBody); break;
	case EKnightRPGPanelTab::Runes: BuildRunes(PageBody); break;
	case EKnightRPGPanelTab::Inventory: BuildGearInventory(PageBody); break;
	case EKnightRPGPanelTab::Campaign: BuildCampaign(PageBody); break;
	case EKnightRPGPanelTab::Journal: BuildJournal(PageBody); break;
	case EKnightRPGPanelTab::Map: BuildCampaignMap(PageBody); break;
	case EKnightRPGPanelTab::Settings: BuildSettings(PageBody); break;
	}
	ApplyCampaignTextScale();
	if (LoadoutName != nullptr)
	{
		LoadoutName->SetText(NameEntry);
	}
	if (bRestoreNameFocus && LoadoutName != nullptr && LoadoutName->GetIsEnabled())
	{
		DeferPageFocus(LoadoutName);
	}
	else if (bRestoreFocus)
	{
		UWidget* Focus = nullptr;
		for (const TObjectPtr<UKnightRPGActionButton>& Button : Buttons)
		{
			if (Button != nullptr && Button->GetIsEnabled()
				&& (!FocusedIdentity.IsNone() ? Button->GetFocusIdentity() == FocusedIdentity
					: !FocusedCaption.IsEmpty() && Button->GetCaption().EqualTo(FocusedCaption)))
			{
				Focus = Button;
				break;
			}
		}
		if (Focus == nullptr)
		{
			Focus = NativeGetDesiredFocusTarget();
		}
		if (Focus != nullptr)
		{
			DeferPageFocus(Focus);
		}
	}
}

void UKnightRPGPanel::BuildCharacter(UVerticalBox* Parent)
{
	if (Snapshot.Gear.bEnabled)
	{
		BuildGearCharacter(Parent);
		return;
	}
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Columns);
	UVerticalBox* Doll = KnightRPGPresentation::AddColumn(WidgetTree, Columns, 1.0f);
	UVerticalBox* StatsColumn = KnightRPGPresentation::AddColumn(WidgetTree, Columns, 1.0f);
	UComboBoxString* KnightSelector = WidgetTree->ConstructWidget<UComboBoxString>();
	for (const FKnightRPGKnightChoice& Knight : Snapshot.Knights)
	{
		KnightSelector->AddOption(Knight.Name.ToString());
		if (Knight.KnightId == Snapshot.KnightId)
		{
			KnightSelector->SetSelectedOption(Knight.Name.ToString());
		}
	}
	KnightSelector->OnSelectionChanged.AddDynamic(this, &UKnightRPGPanel::HandleKnightSelected);
	Doll->AddChildToVerticalBox(KnightSelector);
	AddLine(Doll, NSLOCTEXT("KnightRPGUI", "DollLabel", "GOLEM • native graybox model preview"), 16);
	UHorizontalBox* DollColumns = WidgetTree->ConstructWidget<UHorizontalBox>();
	Doll->AddChildToVerticalBox(DollColumns);
	UVerticalBox* LeftSlots = KnightRPGPresentation::AddColumn(WidgetTree, DollColumns, 0.7f);
	UVerticalBox* ModelColumn = KnightRPGPresentation::AddColumn(WidgetTree, DollColumns, 1.2f);
	UVerticalBox* RightSlots = KnightRPGPresentation::AddColumn(WidgetTree, DollColumns, 0.7f);
	AddLine(LeftSlots, NSLOCTEXT("KnightRPGUI", "WeaponSlot", "WEAPON"), 14);
	AddAction(LeftSlots, EKnightRPGPanelAction::EquipWeapon, NSLOCTEXT("KnightRPGUI", "EquipWeapon", "Equip available weapon"));
	AddAction(LeftSlots, EKnightRPGPanelAction::UnequipWeapon, NSLOCTEXT("KnightRPGUI", "UnequipWeapon", "Unequip weapon"));
	AddLine(LeftSlots, Snapshot.WeaponComparison, 12);
	AddLine(RightSlots, NSLOCTEXT("KnightRPGUI", "RuneSlot", "M5 RUNE SLOT"), 14);
	AddAction(RightSlots, EKnightRPGPanelAction::EquipRune, NSLOCTEXT("KnightRPGUI", "EquipRune", "Equip available rune"));
	AddAction(RightSlots, EKnightRPGPanelAction::UnequipRune, NSLOCTEXT("KnightRPGUI", "UnequipRune", "Unequip rune"));
	AddLine(RightSlots, Snapshot.RuneComparison, 12);
	UKnightRPGActionButton* Sockets = MakeButton(NSLOCTEXT("KnightRPGUI", "AttachSockets", "Attach Runes →"));
	Sockets->OnClicked().AddWeakLambda(this, [this]()
	{
		SetActiveTab(EKnightRPGPanelTab::Runes);
	});
	RightSlots->AddChildToVerticalBox(Sockets);
	USizeBox* PreviewSize = WidgetTree->ConstructWidget<USizeBox>();
	PreviewSize->SetHeightOverride(285.0f);
	ModelColumn->AddChildToVerticalBox(PreviewSize);
	PaperDollImage = WidgetTree->ConstructWidget<UImage>();
	PreviewSize->SetContent(PaperDollImage);
	if (PreviewTarget != nullptr)
	{
		PaperDollImage->SetBrushResourceObject(PreviewTarget);
	}
	else if (IsActivated())
	{
		CreatePaperDollPreview();
	}
	AddLine(Doll, Snapshot.Equipment, 14);
	AddLine(Doll, Snapshot.Inventory, 13);
	AddAction(Doll, EKnightRPGPanelAction::PurchaseHammerSkill,
		NSLOCTEXT("KnightRPGUI", "LegacyHammer", "M5 upgrade: Hammer Force"));
	AddAction(Doll, EKnightRPGPanelAction::PurchaseRuneSkill,
		NSLOCTEXT("KnightRPGUI", "LegacyRune", "M5 upgrade: Rune Focus"));
	AddAction(Doll, EKnightRPGPanelAction::Respec,
		NSLOCTEXT("KnightRPGUI", "LegacyRefund", "Refund M5 purchased upgrades"));
	AddLine(StatsColumn, NSLOCTEXT("KnightRPGUI", "Attributes", "ATTRIBUTES • confirmed"), 20);
	FText LastGroup;
	for (const FKnightRPGStatRow& Row : Snapshot.StatRows)
	{
		if (!Row.Group.EqualTo(LastGroup))
		{
			AddLine(StatsColumn, Row.Group, 15);
			LastGroup = Row.Group;
		}
		UKnightRPGActionButton* Value = MakeButton(
			FText::Format(NSLOCTEXT("KnightRPGUI", "StatRow", "{0}:  {1}"), Row.Name, Row.Value));
		Value->SetCompact(true);
		Value->SetToolTipText(Row.Contributions);
		const int32 RowIndex = StatValueButtons.Num();
		Value->OnInspectRequested.AddWeakLambda(this, [this, RowIndex]()
		{
			if (Snapshot.StatRows.IsValidIndex(RowIndex))
			{
				ShowDetail(Snapshot.StatRows[RowIndex].Contributions);
			}
		});
		Value->OnClicked().AddWeakLambda(this, [this, RowIndex]()
		{
			if (Snapshot.StatRows.IsValidIndex(RowIndex))
			{
				ShowDetail(Snapshot.StatRows[RowIndex].Contributions);
			}
		});
		StatsColumn->AddChildToVerticalBox(Value);
		StatValueButtons.Add(Value);
	}
	if (Snapshot.StatRows.IsEmpty())
	{
		AddLine(StatsColumn, Snapshot.Stats);
	}
	AddLine(StatsColumn, NSLOCTEXT("KnightRPGUI", "StatSources", "Hover a stat for base / equipment / talent / rune / buff contributions and caps."), 13);
}

void UKnightRPGPanel::BuildSpecialization(UVerticalBox* Parent)
{
	AddLine(Parent, NSLOCTEXT("KnightRPGUI", "SpecHint", "SPECIALIZATION • stage one of this Golem's three roles; Apply at sanctuary"), 20);
	for (const FKnightRPGSpecRow& Spec : Snapshot.Specializations)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Parent->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 6.0f));
		UKnightRPGActionButton* Select = MakeButton(FText::Format(
			NSLOCTEXT("KnightRPGUI", "SpecName", "{0} / {1}"), Spec.Name, Spec.Role), Spec.Summary);
		Select->SetMinDimensions(230, 88);
		Select->SetIsEnabled(Spec.bUnlocked && CanStage());
		Select->SetPresentationState(Draft.SpecId == Spec.SpecId,
			bHasDraft && Draft.SpecId == Spec.SpecId && Snapshot.Build.SpecId != Spec.SpecId);
		const FName SpecId = Spec.SpecId;
		Select->OnClicked().AddWeakLambda(this, [this, SpecId]()
		{
			StageSpec(SpecId);
		});
		Buttons.Add(Select);
		Row->AddChildToHorizontalBox(Select);
		UVerticalBox* Description = WidgetTree->ConstructWidget<UVerticalBox>();
		Row->AddChildToHorizontalBox(Description)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		if (!Spec.Summary.EqualTo(Spec.PassiveTooltip))
		{
			AddLine(Description, Spec.Summary, 15);
		}
		AddLine(Description, FText::Format(NSLOCTEXT("KnightRPGUI", "SpecPassive", "● {0}: {1}"),
			Spec.PassiveName, Spec.PassiveTooltip), 14);
		AddLine(Description, FText::Format(NSLOCTEXT("KnightRPGUI", "SpecBuff", "■ A4: {0}"), Spec.MajorBuff), 14);
	}
}

const FKnightRPGSpecRow* UKnightRPGPanel::FindDraftSpec() const
{
	return Snapshot.Specializations.FindByPredicate([this](const FKnightRPGSpecRow& Row)
	{
		return Row.SpecId == Draft.SpecId;
	});
}

void UKnightRPGPanel::BuildTalents(UVerticalBox* Parent)
{
	const int32 ConfirmedSpent = (!Snapshot.Build.ClassChoice.IsNone() ? 1 : 0) +
		(!Snapshot.Build.SpecChoice.IsNone() ? 1 : 0);
	const int32 DraftSpent = (!Draft.ClassChoice.IsNone() ? 1 : 0) + (!Draft.SpecChoice.IsNone() ? 1 : 0);
	AddLine(Parent, FText::Format(NSLOCTEXT("KnightRPGUI", "SharedPoints",
		"Shared point pool • available {0} • draft available {1} • class {2}/1 • spec {3}/1"),
		FText::AsNumber(Snapshot.AvailablePoints), FText::AsNumber(Snapshot.AvailablePoints + ConfirmedSpent - DraftSpent),
		FText::AsNumber(Draft.ClassChoice.IsNone() ? 0 : 1), FText::AsNumber(Draft.SpecChoice.IsNone() ? 0 : 1)), 16);
	AddLine(Parent, Preview.Summary, 13);
	UHorizontalBox* Trees = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Trees);
	BuildTalentTree(Trees, true);
	BuildTalentTree(Trees, false);
	UHorizontalBox* Loadouts = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Loadouts)->SetPadding(FMargin(4.0f, 5.0f));
	LoadoutSelector = WidgetTree->ConstructWidget<UComboBoxString>();
	LoadoutSelector->AddOption(TEXT("Effective build"));
	LoadoutSelector->SetSelectedOption(TEXT("Effective build"));
	for (const FKnightRPGTalentLoadoutRow& Loadout : Snapshot.TalentLoadouts)
	{
		LoadoutSelector->AddOption(Loadout.Name);
		if (SelectedLoadoutId == Loadout.LoadoutId)
		{
			LoadoutSelector->SetSelectedOption(Loadout.Name);
		}
	}
	LoadoutSelector->SetIsEnabled(!bPending);
	LoadoutSelector->OnSelectionChanged.AddDynamic(this, &UKnightRPGPanel::HandleLoadoutSelected);
	Loadouts->AddChildToHorizontalBox(LoadoutSelector)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LoadoutName = WidgetTree->ConstructWidget<UEditableTextBox>();
	LoadoutName->SetHintText(NSLOCTEXT("KnightRPGUI", "PresetName", "Loadout name"));
	LoadoutName->SetIsEnabled(CanStage());
	Loadouts->AddChildToHorizontalBox(LoadoutName)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	const EKnightRPGPanelAction Actions[] =
	{
		EKnightRPGPanelAction::NewTalentLoadout, EKnightRPGPanelAction::RenameTalentLoadout,
		EKnightRPGPanelAction::DuplicateTalentLoadout, EKnightRPGPanelAction::DeleteTalentLoadout,
		EKnightRPGPanelAction::ApplyTalentLoadout
	};
	const FText Captions[] =
	{
		NSLOCTEXT("KnightRPGUI", "NewPreset", "New"), NSLOCTEXT("KnightRPGUI", "RenamePreset", "Rename"),
		NSLOCTEXT("KnightRPGUI", "DuplicatePreset", "Duplicate"), NSLOCTEXT("KnightRPGUI", "DeletePreset", "Delete"),
		NSLOCTEXT("KnightRPGUI", "ActivatePreset", "Activate")
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Actions); ++Index)
	{
		const EKnightRPGPanelAction Action = Actions[Index];
		const FText Tooltip = Action == EKnightRPGPanelAction::ApplyTalentLoadout ?
			NSLOCTEXT("KnightRPGUI", "ActivatePresetTip",
				"Activate this saved talent build atomically. Current equipment and both Attach Rune sockets are preserved; incompatible retained runes are rejected.") :
			NSLOCTEXT("KnightRPGUI", "PresetStorageTip",
				"Talent loadouts store specialization and class/spec choices in the M5 snapshot. Equipment and runes are excluded.");
		UKnightRPGActionButton* Button = MakeButton(Captions[Index], Tooltip);
		Button->SetIsEnabled(CanStage() && (Action == EKnightRPGPanelAction::NewTalentLoadout ||
			SelectedLoadoutId.IsValid()));
		Button->OnClicked().AddWeakLambda(this, [this, Action]()
		{
			SubmitCommand(Action);
		});
		Loadouts->AddChildToHorizontalBox(Button)->SetPadding(FMargin(3.0f, 0.0f));
	}
	AddLine(Parent, NSLOCTEXT("KnightRPGUI", "TalentLegend",
		"■ Active  ● Passive  ◆ Choice   |   Core = free   Gold = confirmed   Cyan = draft   Right click / X = refund"), 13);
}

void UKnightRPGPanel::BuildTalentTree(UHorizontalBox* Parent, bool bClassTree)
{
	UVerticalBox* Column = KnightRPGPresentation::AddColumn(WidgetTree, Parent);
	const FKnightRPGSpecRow* Spec = FindDraftSpec();
	AddLine(Column, bClassTree ? NSLOCTEXT("KnightRPGUI", "ClassTree", "CLASS TALENTS") :
		FText::Format(NSLOCTEXT("KnightRPGUI", "SpecTree", "{0} • SPECIALIZATION"), FText::FromName(Draft.SpecId)), 19);
	USizeBox* TreeSize = WidgetTree->ConstructWidget<USizeBox>();
	TreeSize->SetHeightOverride(374.0f);
	Column->AddChildToVerticalBox(TreeSize);
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	TreeSize->SetContent(Canvas);
	if (bClassTree)
	{
		AddTreeNode(Canvas, FVector2D(80.0f, 40.0f), 0,
			NSLOCTEXT("KnightRPGUI", "AttackCore", "Light / Heavy\nCore"),
			NSLOCTEXT("KnightRPGUI", "AttackCoreTip", "Core • free. Light and heavy hammer attacks use the existing GAS combat pipeline."), false, true);
		AddTreeNode(Canvas, FVector2D(265.0f, 40.0f), 0,
			NSLOCTEXT("KnightRPGUI", "DefenseCore", "Block / Parry\nCore"),
			NSLOCTEXT("KnightRPGUI", "DefenseCoreTip", "Core • free. Directional block and timed parry retain the M3 resource rules."), false, true);
		AddTreeNode(Canvas, FVector2D(450.0f, 40.0f), 0,
			NSLOCTEXT("KnightRPGUI", "MovementCore", "Dodge / Interrupt\nCore"),
			NSLOCTEXT("KnightRPGUI", "MovementCoreTip", "Core • free. Movement and interrupt are separate from four specialization slots."), false, true);
	}
	else
	{
		const TArray<FKnightRPGSpellRow> EmptySpells;
		const TArray<FKnightRPGSpellRow>& Spells = Preview.Build == Draft ? Preview.Spells :
			(Draft == Snapshot.Build ? Snapshot.Spellbook : EmptySpells);
		for (const FKnightRPGSpellRow& Spell : Spells)
		{
			if (!Spell.bKnown || Spell.Slot < 1 || Spell.Slot > 4)
			{
				continue;
			}
			const float X = Spell.Slot % 2 == 1 ? 115.0f : 415.0f;
			const float Y = Spell.Slot <= 2 ? 45.0f : 180.0f;
			AddTreeNode(Canvas, FVector2D(X, Y), 0,
				FText::Format(NSLOCTEXT("KnightRPGUI", "SpellCoreNode", "A{0}\nCore"),
					FText::AsNumber(Spell.Slot)),
				FText::Format(NSLOCTEXT("KnightRPGUI", "NamedSpellTooltip", "{0}\n{1}"), Spell.Name, Spell.Tooltip),
				false, false, Spell.Name);
		}
		if (Spec != nullptr)
		{
			AddTreeNode(Canvas, FVector2D(265.0f, 45.0f), 1, NSLOCTEXT("KnightRPGUI", "PassiveCore", "Passive\nCore"),
				FText::Format(NSLOCTEXT("KnightRPGUI", "NamedSpellTooltip", "{0}\n{1}"),
					Spec->PassiveName, Spec->PassiveTooltip), false, false, Spec->PassiveName);
		}
	}
	const FName Choice = bClassTree ? Draft.ClassChoice : Draft.SpecChoice;
	AddTreeNode(Canvas, FVector2D(265.0f, 277.0f), 2, KnightRPGPresentation::ChoiceLabel(Choice),
		bClassTree ? NSLOCTEXT("KnightRPGUI", "ClassChoiceTip",
			"Choice • rank 0/1 or 1/1 • one shared point. Endurance: +10 MaxStamina; Reservoir: +10 MaxResource. No prerequisites.") :
		FText::Format(NSLOCTEXT("KnightRPGUI", "SpecChoiceTip",
			"Choice • rank 0/1 or 1/1 • one shared point. A: {0}. B: {1}. No prerequisites."),
			Spec == nullptr ? FText() : Spec->ChoiceA, Spec == nullptr ? FText() : Spec->ChoiceB), true, bClassTree,
		Choice.IsNone() ? NSLOCTEXT("KnightRPGUI", "UnspentChoice", "Unspent") : FText::FromName(Choice));
	const bool bShowChoices = bClassTree ? bShowClassChoices : bShowSpecChoices;
	// These branches expose the real mutually exclusive options; core nodes have no prerequisites.
	if (bShowChoices)
	{
		KnightRPGPresentation::AddChoiceConnector(WidgetTree, Canvas, FVector2D(235.0f, 295.0f),
			FVector2D(132.0f, 374.0f));
		KnightRPGPresentation::AddChoiceConnector(WidgetTree, Canvas, FVector2D(295.0f, 295.0f),
			FVector2D(398.0f, 374.0f));
		Canvas->SetToolTipText(NSLOCTEXT("KnightRPGUI", "ChoiceBranchesTooltip",
			"Choice branches connect mutually exclusive A/B options. Choose one at rank 1/1; no prerequisite relationship."));
	}
	UHorizontalBox* Flyout = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Flyout);
	Flyout->SetVisibility(bShowChoices ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	const FText Labels[] =
	{
		bClassTree ? NSLOCTEXT("KnightRPGUI", "Endurance", "Endurance\n+10 Stamina") :
			(Spec == nullptr ? FText() : Spec->ChoiceA),
		bClassTree ? NSLOCTEXT("KnightRPGUI", "Reservoir", "Reservoir\n+10 Resource") :
			(Spec == nullptr ? FText() : Spec->ChoiceB),
		NSLOCTEXT("KnightRPGUI", "Refund", "Refund")
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Labels); ++Index)
	{
		UKnightRPGActionButton* Button = MakeButton(Labels[Index]);
		if (Index == 0)
		{
			if (bClassTree)
			{
				ClassChoiceFirstButton = Button;
			}
			else
			{
				SpecChoiceFirstButton = Button;
			}
		}
		Button->SetIsEnabled(CanStage());
		const FName Option = Index == 2 ? NAME_None :
			FName(bClassTree ? (Index == 0 ? TEXT("Endurance") : TEXT("Reservoir")) :
				(Index == 0 ? TEXT("A") : TEXT("B")));
		Button->SetPresentationState(Choice == Option && !Option.IsNone(),
			bHasDraft && Choice == Option && Choice != (bClassTree ? Snapshot.Build.ClassChoice : Snapshot.Build.SpecChoice));
		Button->OnClicked().AddWeakLambda(this, [this, bClassTree, Option]()
		{
			StageChoice(bClassTree, Option);
		});
		if (Index < 2)
		{
			Flyout->AddChildToHorizontalBox(Button)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		else
		{
			Button->SetVisibility(bShowChoices ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			Column->AddChildToVerticalBox(Button);
		}
		Buttons.Add(Button);
	}
}

void UKnightRPGPanel::AddTreeNode(UCanvasPanel* Canvas, const FVector2D& Position, uint8 Shape,
	const FText& Name, const FText& Tooltip, bool bChoice, bool bClassTree, const FText& UnderLabel)
{
	UKnightRPGActionButton* Node = MakeButton(Name, Tooltip);
	Node->SetNodeShape(Shape);
	Node->SetCompact(true);
	const FName Choice = bClassTree ? Draft.ClassChoice : Draft.SpecChoice;
	const FName Confirmed = bClassTree ? Snapshot.Build.ClassChoice : Snapshot.Build.SpecChoice;
	Node->SetPresentationState(!bChoice || !Choice.IsNone(), bChoice && Choice != Confirmed);
	UCanvasPanelSlot* NodeSlot = Canvas->AddChildToCanvas(Node);
	NodeSlot->SetPosition(Position - FVector2D(45.0f));
	NodeSlot->SetSize(Shape == 2 ? FVector2D(60.0f) : FVector2D(90.0f));
	if (Shape == 2)
	{
		NodeSlot->SetPosition(Position - FVector2D(30.0f));
	}
	if (!UnderLabel.IsEmpty())
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(UnderLabel);
		Label->SetAutoWrapText(true);
		Label->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 11;
		Label->SetFont(Font);
		UCanvasPanelSlot* LabelSlot = Canvas->AddChildToCanvas(Label);
		LabelSlot->SetPosition(Position + FVector2D(-80.0f, Shape == 2 ? 40.0f : 47.0f));
		LabelSlot->SetSize(FVector2D(160.0f, 43.0f));
	}
	Node->OnInspectRequested.AddWeakLambda(this, [this, bClassTree, bChoice]()
	{
		if (bChoice)
		{
			bClassChoiceFocused = bClassTree;
		}
	});
	Node->OnClicked().AddWeakLambda(this, [this, Tooltip, bClassTree, bChoice]()
	{
		ShowDetail(Tooltip);
		if (bChoice)
		{
			bClassChoiceFocused = bClassTree;
			if (bClassTree)
			{
				bShowClassChoices = true;
			}
			else
			{
				bShowSpecChoices = true;
			}
			RebuildActivePage();
			UKnightRPGActionButton* Focus = bClassTree ? ClassChoiceFirstButton.Get() : SpecChoiceFirstButton.Get();
			if (Focus != nullptr)
			{
				Focus->SetUserFocus(GetOwningPlayer());
			}
		}
	});
	if (bChoice)
	{
		Node->OnRefundRequested.AddWeakLambda(this, [this, bClassTree]()
		{
			StageChoice(bClassTree, NAME_None);
		});
	}
	Buttons.Add(Node);
}

void UKnightRPGPanel::BuildSpellbook(UVerticalBox* Parent)
{
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Columns);
	UVerticalBox* Categories = KnightRPGPresentation::AddColumn(WidgetTree, Columns, 0.35f);
	UVerticalBox* List = KnightRPGPresentation::AddColumn(WidgetTree, Columns, 1.0f);
	UVerticalBox* Description = KnightRPGPresentation::AddColumn(WidgetTree, Columns, 1.0f);
	const FName CategoryIds[] = {TEXT("General"), TEXT("Class"), TEXT("Spec"), TEXT("Passives")};
	for (const FName Category : CategoryIds)
	{
		UKnightRPGActionButton* Button = MakeButton(FText::FromName(Category));
		Button->SetPresentationState(Category == SpellCategory, false);
		Button->OnClicked().AddWeakLambda(this, [this, Category]()
		{
			SpellCategory = Category;
			SelectedSpellId = NAME_None;
			RebuildActivePage();
		});
		Categories->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 5.0f));
	}
	AddLine(List, NSLOCTEXT("KnightRPGUI", "KnownSkills", "SPELLBOOK • known / unlearned"), 18);
	const FKnightRPGSpellRow* Selected = nullptr;
	for (const FKnightRPGSpellRow& Spell : Snapshot.Spellbook)
	{
		if (Spell.Category != SpellCategory)
		{
			continue;
		}
		if (SelectedSpellId.IsNone())
		{
			SelectedSpellId = Spell.AbilityId;
		}
		if (SelectedSpellId == Spell.AbilityId)
		{
			Selected = &Spell;
		}
		UKnightRPGActionButton* Button = MakeButton(FText::Format(NSLOCTEXT("KnightRPGUI", "SpellList",
			"{0}  {1}\n{2}"), Spell.bPassive ? NSLOCTEXT("KnightRPGUI", "PassiveGlyph", "●") :
			NSLOCTEXT("KnightRPGUI", "ActiveGlyph", "■"), Spell.Name, Spell.Summary), Spell.Tooltip);
		Button->SetPresentationState(Spell.AbilityId == SelectedSpellId, false);
		const FName AbilityId = Spell.AbilityId;
		Button->OnClicked().AddWeakLambda(this, [this, AbilityId]()
		{
			SelectedSpellId = AbilityId;
			RebuildActivePage();
		});
		List->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 5.0f));
	}
	if (Selected != nullptr)
	{
		AddLine(Description, Selected->Name, 21);
		AddLine(Description, Selected->Tooltip, 15);
		AddLine(Description, Selected->bKnown ? NSLOCTEXT("KnightRPGUI", "Known", "Known") :
			NSLOCTEXT("KnightRPGUI", "Unknown", "Unlearned • activate its specialization"), 14);
		const FText SlotText = Selected->Slot > 0 ?
			FText::Format(NSLOCTEXT("KnightRPGUI", "FixedSlot",
				"Assigned to A{0} • seed slot restriction\nThis skill uses its defined slot. Other slots are incompatible."),
				FText::AsNumber(Selected->Slot)) :
			NSLOCTEXT("KnightRPGUI", "NoActionSlot", "Passive / general action • no A1–A4 assignment");
		AddLine(Description, SlotText, 14);
		if (Selected->Slot > 0)
		{
			UKnightRPGActionButton* Rune = MakeButton(NSLOCTEXT("KnightRPGUI", "SkillRunes", "View compatible Attach Runes →"));
			Rune->OnClicked().AddWeakLambda(this, [this]()
			{
				SetActiveTab(EKnightRPGPanelTab::Runes);
			});
			Description->AddChildToVerticalBox(Rune);
		}
	}
	UHorizontalBox* Bar = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Bar)->SetPadding(FMargin(4.0f, 8.0f));
	for (int32 ActionSlot = 1; ActionSlot <= 4; ++ActionSlot)
	{
		const FKnightRPGSpellRow* Spell = Snapshot.Spellbook.FindByPredicate([ActionSlot](const FKnightRPGSpellRow& Row)
		{
			return Row.bKnown && Row.Slot == ActionSlot;
		});
		UKnightRPGActionButton* Button = MakeButton(FText::Format(NSLOCTEXT("KnightRPGUI", "ActionSlot", "A{0}  {1}"),
			FText::AsNumber(ActionSlot), Spell == nullptr ? FText() : Spell->Name));
		if (Spell != nullptr)
		{
			const FName AbilityId = Spell->AbilityId;
			Button->OnClicked().AddWeakLambda(this, [this, AbilityId]()
			{
				SpellCategory = TEXT("Spec");
				SelectedSpellId = AbilityId;
				RebuildActivePage();
			});
		}
		Bar->AddChildToHorizontalBox(Button)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
}

void UKnightRPGPanel::BuildRunes(UVerticalBox* Parent)
{
	AddLine(Parent, NSLOCTEXT("KnightRPGUI", "RuneTitle", "ATTACH RUNES • equipment / skill sockets"), 20);
	AddLine(Parent, NSLOCTEXT("KnightRPGUI", "RuneDistinction",
		"Shared inventory items. Runestone is a separate world interaction that creates pickups, crystal or brittle armor."), 14);
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Columns);
	UVerticalBox* Inventory = KnightRPGPresentation::AddColumn(WidgetTree, Columns, 0.8f);
	UVerticalBox* Current = KnightRPGPresentation::AddColumn(WidgetTree, Columns, 1.0f);
	UVerticalBox* Proposed = KnightRPGPresentation::AddColumn(WidgetTree, Columns, 1.0f);
	AddLine(Inventory, NSLOCTEXT("KnightRPGUI", "SocketInventory", "RUNES / source instances"), 16);
	const TArray<FKnightRPGRuneRow>& Runes = Preview.Build == Draft ? Preview.Runes : Snapshot.Runes;
	for (const FKnightRPGRuneRow& Rune : Runes)
	{
		UKnightRPGActionButton* Button = MakeButton(FText::Format(NSLOCTEXT("KnightRPGUI", "RuneList",
			"{0}\n{1}"), Rune.Name, Rune.bCompatible ?
				NSLOCTEXT("KnightRPGUI", "Compatible", "Compatible") : Rune.IncompatibilityReason), Rune.Description);
		Button->SetIsEnabled(CanStage() && Rune.bCompatible);
		Button->SetPresentationState(Rune.RuneId == (Rune.bWeaponSocket ? Draft.WeaponRune : Draft.SkillRune),
			bHasDraft && Rune.RuneId == (Rune.bWeaponSocket ? Draft.WeaponRune : Draft.SkillRune));
		Button->OnClicked().AddWeakLambda(this, [this, Rune]()
		{
			StageRune(Rune);
		});
		Inventory->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 4.0f));
		Buttons.Add(Button);
	}
	UKnightRPGActionButton* ClearWeapon = MakeButton(NSLOCTEXT("KnightRPGUI", "ClearWeaponSocket", "Clear weapon socket"));
	ClearWeapon->SetIsEnabled(CanStage());
	ClearWeapon->OnClicked().AddWeakLambda(this, [this]()
	{
		FKnightRPGRuneRow Rune;
		Rune.bWeaponSocket = true;
		Rune.bCompatible = true;
		StageRune(Rune);
	});
	Inventory->AddChildToVerticalBox(ClearWeapon);
	Buttons.Add(ClearWeapon);
	UKnightRPGActionButton* ClearSkill = MakeButton(NSLOCTEXT("KnightRPGUI", "ClearSkillSocket", "Clear skill socket"));
	ClearSkill->SetIsEnabled(CanStage());
	ClearSkill->OnClicked().AddWeakLambda(this, [this]()
	{
		FKnightRPGRuneRow Rune;
		Rune.bCompatible = true;
		StageRune(Rune);
	});
	Inventory->AddChildToVerticalBox(ClearSkill);
	Buttons.Add(ClearSkill);
	AddLine(Current, NSLOCTEXT("KnightRPGUI", "ConfirmedRune", "CURRENT • confirmed"), 17);
	AddLine(Current, FText::Format(NSLOCTEXT("KnightRPGUI", "SocketValues", "Weapon: {0}\nSkill: {1}"),
		FText::FromName(Snapshot.Build.WeaponRune), FText::FromName(Snapshot.Build.SkillRune)), 14);
	AddLine(Proposed, NSLOCTEXT("KnightRPGUI", "DraftRune", "AFTER ATTACH • pure preview"), 17);
	AddLine(Proposed, FText::Format(NSLOCTEXT("KnightRPGUI", "SocketValues", "Weapon: {0}\nSkill: {1}"),
		FText::FromName(Draft.WeaponRune), FText::FromName(Draft.SkillRune)), 14);
	for (const FKnightRPGSpellRow& Spell : Snapshot.Spellbook)
	{
		if (Spell.bKnown && Spell.Slot >= 2 && Spell.Slot <= 3)
		{
			AddLine(Current, Spell.Name, 16);
			AddLine(Current, Spell.Tooltip, 13);
			const FKnightRPGSpellRow* After = Preview.Build == Draft ?
				Preview.Spells.FindByPredicate([&Spell](const FKnightRPGSpellRow& Row)
			{
				return Row.Slot == Spell.Slot;
			}) : nullptr;
			if (After != nullptr)
			{
				AddLine(Proposed, After->Name, 16);
				AddLine(Proposed, After->Tooltip, 13);
			}
		}
	}
	AddLine(Proposed, Preview.Summary, 13);
}

bool UKnightRPGPanel::CanStage() const
{
	return Snapshot.bCanEditBuild && !bPending;
}

bool UKnightRPGPanel::IsDraftDifferent() const
{
	return !(Draft == Snapshot.Build) || DraftWeaponRuneInstanceId != Snapshot.WeaponRuneInstanceId ||
		DraftSkillRuneInstanceId != Snapshot.SkillRuneInstanceId;
}

void UKnightRPGPanel::StageChoice(bool bClassTree, FName Choice)
{
	if (!CanStage())
	{
		return;
	}
	bClassChoiceFocused = bClassTree;
	if (bClassTree)
	{
		Draft.ClassChoice = Choice;
	}
	else
	{
		Draft.SpecChoice = Choice;
	}
	bHasDraft = IsDraftDifferent();
	RequestPreview();
}

void UKnightRPGPanel::StageSpec(FName SpecId)
{
	if (!CanStage())
	{
		return;
	}
	if (Draft.SpecId != SpecId)
	{
		Draft.SpecId = SpecId;
		Draft.SpecChoice = NAME_None;
	}
	bHasDraft = IsDraftDifferent();
	RequestPreview();
}

void UKnightRPGPanel::StageRune(const FKnightRPGRuneRow& Rune)
{
	if (!CanStage() || !Rune.bCompatible)
	{
		return;
	}
	if (Rune.bWeaponSocket)
	{
		Draft.WeaponRune = Rune.RuneId;
		DraftWeaponRuneInstanceId = Rune.ItemInstanceId;
	}
	else
	{
		Draft.SkillRune = Rune.RuneId;
		DraftSkillRuneInstanceId = Rune.ItemInstanceId;
	}
	bHasDraft = IsDraftDifferent();
	RequestPreview();
}

void UKnightRPGPanel::RevertDraft()
{
	if (bPending)
	{
		return;
	}
	ResetGearDraft();
	Draft = Snapshot.Build;
	DraftRevision = Snapshot.Revision;
	DraftWeaponRuneInstanceId = Snapshot.WeaponRuneInstanceId;
	DraftSkillRuneInstanceId = Snapshot.SkillRuneInstanceId;
	bHasDraft = false;
	RequestPreview();
}

void UKnightRPGPanel::RequestPreview()
{
	ResetGearDraft();
	OnPreviewRequested.Broadcast(Draft);
	RebuildActivePage();
	RefreshPresentation();
	if (IsActivated() && ActiveTab == EKnightRPGPanelTab::Talents)
	{
		UKnightRPGActionButton* Focus = bClassChoiceFocused ? ClassChoiceFirstButton.Get() : SpecChoiceFirstButton.Get();
		if (Focus != nullptr)
		{
			Focus->SetUserFocus(GetOwningPlayer());
		}
	}
}

void UKnightRPGPanel::SubmitCommand(EKnightRPGPanelAction Action)
{
	if (!CanStage())
	{
		return;
	}
	FKnightRPGPanelCommand Command;
	Command.Action = Action;
	Command.KnightId = Snapshot.KnightId;
	Command.ExpectedRevision = Action == EKnightRPGPanelAction::ApplyBuild ? DraftRevision : Snapshot.Revision;
	Command.RequestId = FGuid::NewGuid();
	const bool bNamedLoadout = Action == EKnightRPGPanelAction::NewTalentLoadout ||
		Action == EKnightRPGPanelAction::RenameTalentLoadout ||
		Action == EKnightRPGPanelAction::DuplicateTalentLoadout;
	if (bNamedLoadout && LoadoutName != nullptr)
	{
		Command.TalentLoadoutName = LoadoutName->GetText().ToString().TrimStartAndEnd();
	}
	switch (Action)
	{
	case EKnightRPGPanelAction::ApplyBuild:
		Command.ProposedBuild = Draft;
		Command.WeaponRuneInstanceId = DraftWeaponRuneInstanceId;
		Command.SkillRuneInstanceId = DraftSkillRuneInstanceId;
		break;
	case EKnightRPGPanelAction::NewTalentLoadout:
		Command.ProposedBuild = Draft;
		Command.ProposedBuild.WeaponRune = NAME_None;
		Command.ProposedBuild.SkillRune = NAME_None;
		break;
	case EKnightRPGPanelAction::RenameTalentLoadout:
	case EKnightRPGPanelAction::DuplicateTalentLoadout:
	case EKnightRPGPanelAction::DeleteTalentLoadout:
	case EKnightRPGPanelAction::ApplyTalentLoadout:
		Command.TalentLoadoutId = SelectedLoadoutId;
		if (!Command.TalentLoadoutId.IsValid())
		{
			ShowDetail(NSLOCTEXT("KnightRPGUI", "MissingLoadout", "Select an existing talent loadout first."));
			return;
		}
		break;
	default:
		return;
	}
	if (bNamedLoadout && Command.TalentLoadoutName.IsEmpty())
	{
		ShowDetail(NSLOCTEXT("KnightRPGUI", "MissingName", "Enter a loadout name before New, Rename or Duplicate."));
		return;
	}
	if (!OnCommandRequested.IsBound())
	{
		ShowDetail(NSLOCTEXT("KnightRPGUI", "MissingController", "Domain command controller is unavailable."));
		return;
	}
	bPending = true;
	PendingRequestId = Command.RequestId;
	RebuildActivePage();
	RefreshPresentation();
	OnCommandRequested.Broadcast(Command);
}

void UKnightRPGPanel::HandleLoadoutSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bRebuildingPage || bPending)
	{
		return;
	}
	const FKnightRPGTalentLoadoutRow* Loadout = Snapshot.TalentLoadouts.FindByPredicate(
		[&SelectedItem](const FKnightRPGTalentLoadoutRow& Row)
		{
			return Row.Name == SelectedItem;
		});
	SelectedLoadoutId = Loadout == nullptr ? FGuid() : Loadout->LoadoutId;
	if (Loadout != nullptr)
	{
		Draft.SpecId = Loadout->Build.SpecId;
		Draft.ClassChoice = Loadout->Build.ClassChoice;
		Draft.SpecChoice = Loadout->Build.SpecChoice;
		// Talent presets intentionally preserve equipment and both attachment sockets.
		bHasDraft = IsDraftDifferent();
		RequestPreview();
	}
	else
	{
		RevertDraft();
	}
}

void UKnightRPGPanel::HandleKnightSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bRebuildingPage || bPending)
	{
		return;
	}
	const FKnightRPGKnightChoice* Knight = Snapshot.Knights.FindByPredicate(
		[&SelectedItem](const FKnightRPGKnightChoice& Row)
		{
			return Row.Name.ToString() == SelectedItem;
		});
	if (Knight != nullptr && !(Knight->KnightId == Snapshot.KnightId))
	{
		FKnightRPGPanelCommand Command;
		Command.Action = EKnightRPGPanelAction::SelectKnight;
		Command.KnightId = Knight->KnightId;
		OnCommandRequested.Broadcast(Command);
	}
}

void UKnightRPGPanel::ShowDetail(const FText& Text)
{
	if (DetailText != nullptr)
	{
		DetailText->SetText(Text);
		if (DetailScroll != nullptr)
		{
			DetailScroll->ScrollToStart();
		}
	}
}

void UKnightRPGPanel::ScrollControlIntoView(UWidget* Control) const
{
	if (Control == nullptr)
	{
		return;
	}
	UWidget* ScrollTarget = Control;
	for (UPanelWidget* Parent = Control->GetParent(); Parent != nullptr; Parent = Parent->GetParent())
	{
		if (UScrollBox* Scroll = Cast<UScrollBox>(Parent))
		{
			// An outer Slate scroll cannot find a deeply clipped child across another scroll boundary.
			Scroll->ScrollWidgetIntoView(ScrollTarget, false, EDescendantScrollDestination::IntoView);
			ScrollTarget = Scroll;
		}
	}
}

void UKnightRPGPanel::DeferPageFocus(UWidget* Control)
{
	CancelDeferredPageFocus();
	if (Control == nullptr || !IsActivated() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	DeferredPageFocus = Control;
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const TSharedPtr<FSlateUser> User = LocalPlayer != nullptr ? LocalPlayer->GetSlateUser() : nullptr;
	DeferredOriginalFocus = User.IsValid() ?
		FSlateApplication::Get().GetUserFocusedWidget(User->GetUserIndex()) : nullptr;
	// The replacement tree must be laid out before scroll requests use its geometry. A second
	// layout pass also accounts for the outer page moving an inner scroll viewport.
	PageLayoutHandle = FSlateApplication::Get().OnPostTick().AddUObject(this, &ThisClass::HandlePageLayoutFinished);
}

void UKnightRPGPanel::HandlePageLayoutFinished(float DeltaTime)
{
	UWidget* Focus = DeferredPageFocus.Get();
	if (!IsActivated() || Focus == nullptr || !Focus->GetIsEnabled() || !Focus->IsVisible() ||
		GetOwningPlayer() == nullptr)
	{
		CancelDeferredPageFocus();
		return;
	}
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const TSharedPtr<FSlateUser> User = LocalPlayer != nullptr ? LocalPlayer->GetSlateUser() : nullptr;
	const TSharedPtr<SWidget> CurrentFocus = User.IsValid() ?
		FSlateApplication::Get().GetUserFocusedWidget(User->GetUserIndex()) : nullptr;
	const bool bNewPanelFocus = CurrentFocus.IsValid() && CurrentFocus != DeferredOriginalFocus.Pin()
		&& HasUserFocusedDescendants(GetOwningPlayer());
	UE_LOG(LogTemp, Verbose, TEXT("PageFocusLayout target=%s pass=%d newPanelFocus=%d focused=%d"),
		*Focus->GetName(), PageLayoutPasses, bNewPanelFocus,
		Focus->HasUserFocus(GetOwningPlayer()) || Focus->HasUserFocusedDescendants(GetOwningPlayer()));
	if ((PageLayoutPasses > 0 || bNewPanelFocus) && !Focus->HasUserFocus(GetOwningPlayer()) &&
		!Focus->HasUserFocusedDescendants(GetOwningPlayer()))
	{
		// A user move to another valid control supersedes this reconstruction's focus request.
		CancelDeferredPageFocus();
		return;
	}
	if (++PageLayoutPasses <= 2)
	{
		Focus->SetUserFocus(GetOwningPlayer());
		ScrollControlIntoView(Focus);
	}
	else
	{
		CancelDeferredPageFocus();
	}
}

void UKnightRPGPanel::CancelDeferredPageFocus()
{
	if (PageLayoutHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPostTick().Remove(PageLayoutHandle);
	}
	PageLayoutHandle.Reset();
	DeferredPageFocus.Reset();
	DeferredOriginalFocus.Reset();
	PageLayoutPasses = 0;
}

void UKnightRPGPanel::CreatePaperDollPreview()
{
	if (PreviewActor != nullptr || GetWorld() == nullptr)
	{
		return;
	}
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube == nullptr)
	{
		ShowDetail(NSLOCTEXT("KnightRPGUI", "MissingPreviewMesh", "Graybox preview mesh unavailable."));
		return;
	}
	FActorSpawnParameters Parameters;
	Parameters.ObjectFlags = RF_Transient;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	PreviewActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector(0.0f, 0.0f, -50000.0f),
		FRotator::ZeroRotator, Parameters);
	if (PreviewActor == nullptr)
	{
		return;
	}
	PreviewActor->SetReplicates(false);
	PreviewActor->SetActorEnableCollision(false);
	USceneComponent* Root = NewObject<USceneComponent>(PreviewActor);
	PreviewActor->AddInstanceComponent(Root);
	PreviewActor->SetRootComponent(Root);
	Root->RegisterComponent();
	PreviewActor->SetActorLocation(FVector(0.0f, 0.0f, -50000.0f));
	PreviewTarget = NewObject<UTextureRenderTarget2D>(this);
	PreviewTarget->ClearColor = FLinearColor(0.025f, 0.05f, 0.06f, 1.0f);
	PreviewTarget->InitAutoFormat(256, 384);
	PreviewCapture = NewObject<USceneCaptureComponent2D>(PreviewActor);
	PreviewCapture->SetupAttachment(Root);
	const FVector ViewPosition(430.0f, -180.0f, 160.0f);
	PreviewCapture->SetRelativeLocation(ViewPosition);
	PreviewCapture->SetRelativeRotation(FRotationMatrix::MakeFromX(FVector(0.0f, 0.0f, 115.0f) - ViewPosition).Rotator());
	PreviewCapture->ProjectionType = ECameraProjectionMode::Perspective;
	PreviewCapture->FOVAngle = 38.0f;
	PreviewCapture->TextureTarget = PreviewTarget;
	PreviewCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	PreviewCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	PreviewCapture->bCaptureEveryFrame = false;
	PreviewCapture->bCaptureOnMovement = false;
	PreviewCapture->ShowFlags.SetLighting(true);
	PreviewCapture->ShowFlags.SetAtmosphere(false);
	PreviewCapture->ShowFlags.SetFog(false);
	PreviewCapture->ShowFlags.SetDynamicShadows(false);
	PreviewCapture->PostProcessSettings.bOverride_AutoExposureMethod = true;
	PreviewCapture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	PreviewCapture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PreviewCapture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	PreviewActor->AddInstanceComponent(PreviewCapture);
	PreviewCapture->RegisterComponent();
	UPointLightComponent* KeyLight = NewObject<UPointLightComponent>(PreviewActor);
	KeyLight->SetupAttachment(Root);
	KeyLight->SetRelativeLocation(FVector(230.0f, -180.0f, 280.0f));
	KeyLight->SetIntensityUnits(ELightUnits::Lumens);
	KeyLight->SetIntensity(5000.0f);
	KeyLight->SetAttenuationRadius(1200.0f);
	KeyLight->SetLightColor(FLinearColor(0.86f, 0.95f, 1.0f));
	PreviewActor->AddInstanceComponent(KeyLight);
	KeyLight->RegisterComponent();
	const FVector Positions[] =
	{
		FVector(0.0f, 0.0f, 200.0f), FVector(0.0f, 0.0f, 120.0f),
		FVector(0.0f, -62.0f, 120.0f), FVector(0.0f, 62.0f, 120.0f),
		FVector(0.0f, -25.0f, 35.0f), FVector(0.0f, 25.0f, 35.0f)
	};
	const FVector Scales[] =
	{
		FVector(0.65f), FVector(0.65f, 0.85f, 0.95f),
		FVector(0.4f, 0.35f, 0.85f), FVector(0.4f, 0.35f, 0.85f),
		FVector(0.45f, 0.4f, 0.75f), FVector(0.45f, 0.4f, 0.75f)
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Positions); ++Index)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(PreviewActor);
		Part->SetupAttachment(Root);
		Part->SetStaticMesh(Cube);
		Part->SetRelativeLocation(Positions[Index]);
		Part->SetRelativeScale3D(Scales[Index]);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		PreviewActor->AddInstanceComponent(Part);
		Part->RegisterComponent();
		PreviewCapture->ShowOnlyComponent(Part);
	}
	PreviewCapture->CaptureScene();
	if (PaperDollImage != nullptr)
	{
		PaperDollImage->SetBrushResourceObject(PreviewTarget);
	}
}

void UKnightRPGPanel::ReleasePaperDollPreview()
{
	if (PaperDollImage != nullptr)
	{
		PaperDollImage->SetBrush(FSlateBrush());
	}
	if (PreviewActor != nullptr)
	{
		PreviewActor->Destroy();
	}
	PreviewCapture = nullptr;
	PreviewActor = nullptr;
	PreviewTarget = nullptr;
}

void UKnightRPGPanel::NativeOnActivated()
{
	Super::NativeOnActivated();
	CreatePaperDollPreview();
	if (CampaignSnapshot.bEnabled)
	{
		SetCampaignPresentation(CampaignSnapshot);
	}
}

void UKnightRPGPanel::NativeOnDeactivated()
{
	CapturingBinding = NAME_None;
	CancelDeferredPageFocus();
	ReleasePaperDollPreview();
	Super::NativeOnDeactivated();
	OnPanelClosed.Broadcast();
}

void UKnightRPGPanel::NativeDestruct()
{
	CancelDeferredPageFocus();
	ReleasePaperDollPreview();
	Super::NativeDestruct();
}

bool UKnightRPGPanel::NativeOnHandleBackAction()
{
	OnActionRequested.Broadcast(EKnightRPGPanelAction::Close);
	return true;
}

UWidget* UKnightRPGPanel::NativeGetDesiredFocusTarget() const
{
	// CommonUI requests this again during an input-device change, including before dispatching
	// its first Enter/A key. Preserve the current control so that key activates the intended row.
	UWidget* Focused = nullptr;
	if (IsActivated() && WidgetTree != nullptr && GetOwningPlayer() != nullptr)
	{
		WidgetTree->ForEachWidget([this, &Focused](UWidget* Widget)
		{
			if (Widget != nullptr && Widget->GetIsEnabled() && Widget->IsVisible() &&
				(Widget->HasUserFocus(GetOwningPlayer()) || Widget->HasUserFocusedDescendants(GetOwningPlayer())))
			{
				Focused = Widget;
			}
		});
	}
	if (Focused != nullptr)
	{
		return Focused;
	}
	if (CampaignSnapshot.bEnabled && ActiveTab >= EKnightRPGPanelTab::Campaign)
	{
		const int32 Index = static_cast<int32>(ActiveTab) - static_cast<int32>(EKnightRPGPanelTab::Campaign);
		return CampaignTabButtons.IsValidIndex(Index) ? CampaignTabButtons[Index].Get() : nullptr;
	}
	return TabButtons.IsValidIndex(static_cast<int32>(ActiveTab)) ?
		TabButtons[static_cast<int32>(ActiveTab)].Get() : nullptr;
}

FReply UKnightRPGPanel::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (!CapturingBinding.IsNone())
	{
		const FName ActionId = CapturingBinding;
		CapturingBinding = NAME_None;
		if (Key != EKeys::Escape && Key != EKeys::Gamepad_FaceButton_Right)
		{
			OnCampaignKeyRemapRequested.Broadcast(ActionId, Key);
		}
		else
		{
			RebuildActivePage();
		}
		return FReply::Handled();
	}
	const bool bEditingText = LoadoutName != nullptr &&
		(LoadoutName->HasKeyboardFocus() || LoadoutName->HasFocusedDescendants());
	if (Key == EKeys::Escape || (Key == EKeys::I && !bEditingText) || Key == EKeys::Gamepad_Special_Right ||
		Key == EKeys::Gamepad_FaceButton_Right)
	{
		OnActionRequested.Broadcast(EKnightRPGPanelAction::Close);
		return FReply::Handled();
	}
	if (!bEditingText && (Key == EKeys::Delete || Key == EKeys::Gamepad_FaceButton_Left) &&
		ActiveTab == EKnightRPGPanelTab::Talents)
	{
		StageChoice(bClassChoiceFocused, NAME_None);
		return FReply::Handled();
	}
	if (Key == EKeys::F && !bEditingText)
	{
		if (bGearDraft)
		{
			SubmitGearCommand();
		}
		else if (bHasDraft)
		{
			SubmitCommand(EKnightRPGPanelAction::ApplyBuild);
		}
		return FReply::Handled();
	}
	if (!bEditingText && DetailScroll != nullptr &&
		(Key == EKeys::PageUp || Key == EKeys::PageDown ||
			Key == EKeys::Gamepad_LeftTrigger || Key == EKeys::Gamepad_RightTrigger))
	{
		const float Direction = Key == EKeys::PageUp || Key == EKeys::Gamepad_LeftTrigger ? -1.0f : 1.0f;
		DetailScroll->SetScrollOffset(FMath::Clamp(DetailScroll->GetScrollOffset() + Direction * 48.0f,
			0.0f, DetailScroll->GetScrollOffsetOfEnd()));
		return FReply::Handled();
	}
	if (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder)
	{
		const int32 Direction = Key == EKeys::Gamepad_RightShoulder ? 1 : -1;
		if (CampaignSnapshot.bEnabled && ActiveTab >= EKnightRPGPanelTab::Campaign)
		{
			const int32 Index = (static_cast<int32>(ActiveTab) - static_cast<int32>(EKnightRPGPanelTab::Campaign)
				+ Direction + 4) % 4;
			SetActiveTab(static_cast<EKnightRPGPanelTab>(static_cast<int32>(EKnightRPGPanelTab::Campaign) + Index));
			CampaignTabButtons[Index]->SetUserFocus(GetOwningPlayer());
			return FReply::Handled();
		}
		const int32 Count = TabButtons.Num();
		SetActiveTab(static_cast<EKnightRPGPanelTab>((static_cast<int32>(ActiveTab) + Direction + Count) % Count));
		if (TabButtons.IsValidIndex(static_cast<int32>(ActiveTab)))
		{
			TabButtons[static_cast<int32>(ActiveTab)]->SetUserFocus(GetOwningPlayer());
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
