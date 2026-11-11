#include "Widgets/GolemRPGPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Widgets/GolemRPGActionButton.h"

namespace GolemGearUI
{
	UVerticalBox* Column(UWidgetTree* Tree, UHorizontalBox* Parent, float Weight)
	{
		UBorder* Frame = Tree->ConstructWidget<UBorder>();
		Frame->SetPadding(FMargin(8.0f));
		Frame->SetBrushColor(FLinearColor(0.035f, 0.045f, 0.05f));
		UHorizontalBoxSlot* ColumnSlot = Parent->AddChildToHorizontalBox(Frame);
		FSlateChildSize Fill(ESlateSizeRule::Fill);
		Fill.Value = Weight;
		ColumnSlot->SetSize(Fill);
		ColumnSlot->SetPadding(FMargin(3.0f));
		UVerticalBox* Result = Tree->ConstructWidget<UVerticalBox>();
		Frame->SetContent(Result);
		return Result;
	}

	UVerticalBox* ScrollColumn(UWidgetTree* Tree, UHorizontalBox* Parent, float Weight)
	{
		UVerticalBox* Frame = Column(Tree, Parent, Weight);
		USizeBox* Bounds = Tree->ConstructWidget<USizeBox>();
		Bounds->SetHeightOverride(320.0f);
		Frame->AddChildToVerticalBox(Bounds);
		UScrollBox* Scroll = Tree->ConstructWidget<UScrollBox>();
		Scroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
		Bounds->SetContent(Scroll);
		UVerticalBox* Body = Tree->ConstructWidget<UVerticalBox>();
		Scroll->AddChild(Body);
		return Body;
	}

	const FGolemGearSlotRow* FindSlot(const FGolemGearPanelSnapshot& Gear, FName SlotId)
	{
		return Gear.Slots.FindByPredicate([SlotId](const FGolemGearSlotRow& GearSlot)
		{
			return GearSlot.SlotId == SlotId;
		});
	}
}

const FGolemGearCompare& UGolemRPGPanel::GetGearCompare() const
{
	return GearCompare;
}

const FGolemGearPanelIntent& UGolemRPGPanel::GetGearDraft() const
{
	return GearDraft;
}

FGuid UGolemRPGPanel::GetSelectedGearInstanceId() const
{
	return SelectedGearInstanceId;
}

bool UGolemRPGPanel::HasGearDraft() const
{
	return bGearDraft;
}

void UGolemRPGPanel::SetGearCompare(const FGolemGearCompare& InCompare)
{
	if (InCompare.GolemId == Snapshot.Gear.GolemId && InCompare.InstanceId == SelectedGearInstanceId &&
		InCompare.SlotId == GearTargetSlot)
	{
		GearCompare = InCompare;
	}
}

void UGolemRPGPanel::AddGearSlot(UVerticalBox* Parent, const FGolemGearSlotRow& GearSlot)
{
	const FText ItemLabel = GearSlot.InstanceId.IsValid() ? GearSlot.ItemName :
		NSLOCTEXT("GolemGearUI", "EmptySlot", "Empty");
	UGolemRPGActionButton* Button = MakeButton(FText::Format(
		NSLOCTEXT("GolemGearUI", "SlotCaption", "{0}\n{1} {2}"), GearSlot.Name,
		GearSlot.Badge.IsEmpty() ? FText() : FText::Format(NSLOCTEXT("GolemGearUI", "Badge", "[{0}]"), GearSlot.Badge), ItemLabel), GearSlot.Tooltip);
	Button->SetCompact(true);
	Button->SetCaptionFontSize(11);
	Button->SetCaptionColor(GearSlot.QualityColor);
	Button->SetPresentationState(GearTargetSlot == GearSlot.SlotId, bGearDraft && GearDraft.SlotId == GearSlot.SlotId);
	Button->SetIsEnabled(!bPending && !GearSlot.bLocked);
	const FName SlotId = GearSlot.SlotId;
	const FGuid InstanceId = GearSlot.InstanceId;
	Button->OnClicked().AddWeakLambda(this, [this, SlotId, InstanceId]()
	{
		GearTargetSlot = SlotId;
		SelectedGearInstanceId = InstanceId;
		bSelectingVendorStock = false;
		RequestGearCompare();
		RebuildActivePage();
		RefreshPresentation();
	});
	Parent->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 2.0f));
	Buttons.Add(Button);
}

void UGolemRPGPanel::BuildGearCharacter(UVerticalBox* Parent)
{
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Columns);
	UVerticalBox* Doll = GolemGearUI::Column(WidgetTree, Columns, 1.15f);
	UVerticalBox* Stats = GolemGearUI::ScrollColumn(WidgetTree, Columns, 0.85f);
	UComboBoxString* GolemSelector = WidgetTree->ConstructWidget<UComboBoxString>();
	for (const FGolemGearGolemRow& Golem : Snapshot.Gear.Golems)
	{
		GolemSelector->AddOption(Golem.Name.ToString());
		if (Golem.GolemId == Snapshot.Gear.GolemId)
		{
			GolemSelector->SetSelectedOption(Golem.Name.ToString());
		}
	}
	GolemSelector->SetIsEnabled(!bPending);
	GolemSelector->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleGearGolemSelected);
	Doll->AddChildToVerticalBox(GolemSelector);
	AddLine(Doll, FText::Format(NSLOCTEXT("GolemGearUI", "GearLevel", "Level {0} · 12 equipment slots"),
		FText::AsNumber(Snapshot.Gear.Level)), 13);
	UHorizontalBox* PaperDoll = WidgetTree->ConstructWidget<UHorizontalBox>();
	Doll->AddChildToVerticalBox(PaperDoll);
	UVerticalBox* Left = GolemGearUI::Column(WidgetTree, PaperDoll, 1.0f);
	UVerticalBox* Model = GolemGearUI::Column(WidgetTree, PaperDoll, 1.35f);
	UVerticalBox* Right = GolemGearUI::Column(WidgetTree, PaperDoll, 1.0f);
	const FName LeftSlots[] = {TEXT("Equipment.Slot.Head"), TEXT("Equipment.Slot.Shoulders"),
		TEXT("Equipment.Slot.Chest"), TEXT("Equipment.Slot.Wrists"), TEXT("Equipment.Slot.Hands"), TEXT("Equipment.Slot.Waist")};
	const FName RightSlots[] = {TEXT("Equipment.Slot.Feet"), TEXT("Equipment.Slot.Neck"),
		TEXT("Equipment.Slot.Ring1"), TEXT("Equipment.Slot.Ring2"), TEXT("Equipment.Slot.Relic")};
	for (FName SlotId : LeftSlots)
	{
		if (const FGolemGearSlotRow* GearSlot = GolemGearUI::FindSlot(Snapshot.Gear, SlotId))
		{
			AddGearSlot(Left, *GearSlot);
		}
	}
	for (FName SlotId : RightSlots)
	{
		if (const FGolemGearSlotRow* GearSlot = GolemGearUI::FindSlot(Snapshot.Gear, SlotId))
		{
			AddGearSlot(Right, *GearSlot);
		}
	}
	USizeBox* PreviewSize = WidgetTree->ConstructWidget<USizeBox>();
	PreviewSize->SetHeightOverride(235.0f);
	Model->AddChildToVerticalBox(PreviewSize);
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
	if (const FGolemGearSlotRow* Weapon = GolemGearUI::FindSlot(Snapshot.Gear, TEXT("Equipment.Slot.Weapon")))
	{
		AddGearSlot(Model, *Weapon);
	}
	UGolemRPGActionButton* Bag = MakeButton(NSLOCTEXT("GolemGearUI", "OpenBag", "Open shared inventory / compare"));
	Bag->OnClicked().AddWeakLambda(this, [this]()
	{
		SetActiveTab(EGolemRPGPanelTab::Inventory);
	});
	Doll->AddChildToVerticalBox(Bag);
	Buttons.Add(Bag);
	AddLine(Doll, Snapshot.Gear.EditLockReason, 12);
	AddLine(Stats, NSLOCTEXT("GolemGearUI", "Totals", "ATTRIBUTES · confirmed totals"), 18);
	for (const FGolemRPGStatRow& Row : Snapshot.StatRows)
	{
		UGolemRPGActionButton* Value = MakeButton(FText::Format(
			NSLOCTEXT("GolemGearUI", "Stat", "{0}: {1}"), Row.Name, Row.Value), Row.Contributions);
		Value->SetCompact(true);
		Stats->AddChildToVerticalBox(Value);
		Buttons.Add(Value);
	}
	AddLine(Stats, NSLOCTEXT("GolemGearUI", "SlotRule", "Rings are separate targets. Rune sockets and Q1/Q2 are outside the 12 equipment slots."), 12);
	AddLine(Stats, Snapshot.Gear.RelicStatus, 12);
	AddLine(Stats, Snapshot.Gear.Attachments, 12);
	for (int32 Index = 0; Index < Snapshot.Gear.QuickUses.Num(); ++Index)
	{
		const FGolemGearQuickUseRow& Quick = Snapshot.Gear.QuickUses[Index];
		AddLine(Stats, FText::Format(NSLOCTEXT("GolemGearUI", "QuickBinding", "Q{0}: {1} ×{2} · shared potion cooldown30s"),
			FText::AsNumber(Index + 1), Quick.Name, FText::AsNumber(Quick.Count)), 12);
	}
	BuildGearFixtureControls(Parent);
}

void UGolemRPGPanel::BuildGearFixtureControls(UVerticalBox* Parent)
{
	if (!Snapshot.Gear.bHasDemoState)
	{
		return;
	}
	AddLine(Parent, Snapshot.Gear.FixtureSummary, 13);
	UHorizontalBox* Fixtures = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Fixtures);
	const FName Actions[] = {TEXT("NewDemoRun"), TEXT("CatalogLab"), TEXT("ResetCatalogLab"), TEXT("SanctuaryRecovery")};
	const FText Labels[] = {
		NSLOCTEXT("GolemGearUI", "NewDemo", "Fixture: New Demo Run"),
		NSLOCTEXT("GolemGearUI", "CatalogLab", "DEV ONLY: Catalog Lab"),
		NSLOCTEXT("GolemGearUI", "ResetCatalog", "DEV ONLY: Reset Catalog"),
		NSLOCTEXT("GolemGearUI", "Recovery", "Fixture: Sanctuary Recovery")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Actions); ++Index)
	{
		UGolemRPGActionButton* Button = MakeButton(Labels[Index]);
		Button->SetCompact(true);
		Button->SetIsEnabled(!bPending && Snapshot.Gear.bCanRunFixtures);
		const FName Action = Actions[Index];
		Button->OnClicked().AddWeakLambda(this, [this, Action]()
		{
			RequestGearFixture(Action);
		});
		Fixtures->AddChildToHorizontalBox(Button)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Buttons.Add(Button);
	}
	UHorizontalBox* Presets = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Presets)->SetPadding(FMargin(0.0f, 4.0f));
	const FName PresetActions[] = {TEXT("Guard"), TEXT("Mend"), TEXT("Strike"), TEXT("ClaimReward")};
	const FText PresetLabels[] = {NSLOCTEXT("GolemGearUI", "GuardPreset", "DEV ONLY preset: Guard"),
		NSLOCTEXT("GolemGearUI", "MendPreset", "DEV ONLY preset: Mend"),
		NSLOCTEXT("GolemGearUI", "StrikePreset", "DEV ONLY preset: Strike"),
		NSLOCTEXT("GolemGearUI", "ClaimReward", "Claim confirmed encounter reward")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(PresetActions); ++Index)
	{
		UGolemRPGActionButton* Button = MakeButton(PresetLabels[Index]);
		Button->SetCompact(true);
		Button->SetIsEnabled(!bPending && Snapshot.Gear.bCanEdit &&
			(Index == 3 ? Snapshot.Gear.ClaimableRewardId.IsValid() : Snapshot.Gear.bCatalogLab));
		const FName Action = PresetActions[Index];
		Button->OnClicked().AddWeakLambda(this, [this, Action]()
		{
			RequestGearFixture(Action);
		});
		Presets->AddChildToHorizontalBox(Button)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Buttons.Add(Button);
	}
	UGolemRPGActionButton* SnapshotButton = MakeButton(NSLOCTEXT("GolemGearUI", "Reconstruct",
		"Fixture: reconstruct this run from in-memory snapshot"));
	SnapshotButton->SetCompact(true);
	SnapshotButton->SetIsEnabled(!bPending && Snapshot.Gear.bCanRunFixtures);
	SnapshotButton->OnClicked().AddWeakLambda(this, [this]()
	{
		RequestGearFixture(TEXT("SnapshotReconstruct"));
	});
	Parent->AddChildToVerticalBox(SnapshotButton);
	Buttons.Add(SnapshotButton);
}

const FGolemGearItemRow* UGolemRPGPanel::FindSelectedGearItem() const
{
	if (bSelectingVendorStock)
	{
		return Snapshot.Gear.VendorStock.FindByPredicate([this](const FGolemGearItemRow& Item)
		{
			return Item.DefinitionId == SelectedVendorDefinition;
		});
	}
	return Snapshot.Gear.Items.FindByPredicate([this](const FGolemGearItemRow& Item)
	{
		return Item.InstanceId == SelectedGearInstanceId;
	});
}

void UGolemRPGPanel::BuildGearInventory(UVerticalBox* Parent)
{
	AddLine(Parent, FText::Format(NSLOCTEXT("GolemGearUI", "GearBagHeading", "SHARED INVENTORY · inspect {0}, Lv{1} · currency {2}"),
		Snapshot.Gear.GolemName, FText::AsNumber(Snapshot.Gear.Level), FText::AsNumber(Snapshot.Gear.Currency)), 18);
	BuildGearFixtureControls(Parent);
	UHorizontalBox* Categories = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Categories);
	const FName CategoryIds[] = {TEXT("Equipment"), TEXT("AttachRune"), TEXT("Consumable"), TEXT("Material"), TEXT("QuestItem"), TEXT("Junk")};
	const FText Captions[] = {NSLOCTEXT("GolemGearUI", "Equipment", "Equipment"), NSLOCTEXT("GolemGearUI", "Runes", "Runes"),
		NSLOCTEXT("GolemGearUI", "Consumables", "Consumables"), NSLOCTEXT("GolemGearUI", "Materials", "Materials"),
		NSLOCTEXT("GolemGearUI", "Quest", "Quest"), NSLOCTEXT("GolemGearUI", "Junk", "Junk")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CategoryIds); ++Index)
	{
		UGolemRPGActionButton* Button = MakeButton(Captions[Index]);
		Button->SetCompact(true);
		Button->SetPresentationState(GearCategory == CategoryIds[Index], false);
		const FName Category = CategoryIds[Index];
		Button->OnClicked().AddWeakLambda(this, [this, Category]()
		{
			GearCategory = Category;
			GearSubtype = NAME_None;
			RebuildActivePage();
		});
		Categories->AddChildToHorizontalBox(Button)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Buttons.Add(Button);
	}
	UHorizontalBox* Filters = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Filters)->SetPadding(FMargin(0.0f, 5.0f));
	UComboBoxString* Subtype = WidgetTree->ConstructWidget<UComboBoxString>();
	Subtype->AddOption(TEXT("All subtypes"));
	TArray<FString> Subtypes;
	for (const FGolemGearItemRow& Item : Snapshot.Gear.Items)
	{
		if (Item.Category == GearCategory && !Item.Subtype.IsNone())
		{
			Subtypes.AddUnique(Item.Subtype.ToString());
		}
	}
	Subtypes.Sort();
	for (const FString& Option : Subtypes)
	{
		Subtype->AddOption(Option);
	}
	Subtype->SetSelectedOption(GearSubtype.IsNone() ? TEXT("All subtypes") : GearSubtype.ToString());
	Subtype->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleGearSubtypeSelected);
	Filters->AddChildToHorizontalBox(Subtype)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UComboBoxString* Rarity = WidgetTree->ConstructWidget<UComboBoxString>();
	const FString Qualities[] = {TEXT("All rarities"), TEXT("Poor"), TEXT("Common"), TEXT("Uncommon"), TEXT("Rare"), TEXT("Epic")};
	for (const FString& Quality : Qualities)
	{
		Rarity->AddOption(Quality);
	}
	Rarity->SetSelectedOption(GearRarity.IsNone() ? TEXT("All rarities") : GearRarity.ToString());
	Rarity->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleGearRaritySelected);
	Filters->AddChildToHorizontalBox(Rarity)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UComboBoxString* Sort = WidgetTree->ConstructWidget<UComboBoxString>();
	Sort->AddOption(TEXT("Sort name"));
	Sort->AddOption(TEXT("Sort iLvl"));
	Sort->SetSelectedOption(bGearSortByItemLevel ? TEXT("Sort iLvl") : TEXT("Sort name"));
	Sort->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleGearSortSelected);
	Filters->AddChildToHorizontalBox(Sort)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UCheckBox* Usable = WidgetTree->ConstructWidget<UCheckBox>();
	UTextBlock* UsableLabel = WidgetTree->ConstructWidget<UTextBlock>();
	UsableLabel->SetText(NSLOCTEXT("GolemGearUI", "Usable", "Usable by selected Golem"));
	FSlateFontInfo UsableFont = UsableLabel->GetFont();
	UsableFont.Size = 13;
	UsableLabel->SetFont(UsableFont);
	UsableLabel->SetAutoWrapText(true);
	Usable->SetContent(UsableLabel);
	Usable->SetIsChecked(bGearUsableOnly);
	Usable->OnCheckStateChanged.AddDynamic(this, &ThisClass::HandleGearUsableChanged);
	Filters->AddChildToHorizontalBox(Usable)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Content);
	UVerticalBox* Bag = GolemGearUI::ScrollColumn(WidgetTree, Content, 1.0f);
	UVerticalBox* Inspect = GolemGearUI::ScrollColumn(WidgetTree, Content, 1.15f);
	TArray<const FGolemGearItemRow*> Rows;
	for (const FGolemGearItemRow& Item : Snapshot.Gear.Items)
	{
		if (Item.Category == GearCategory && (GearSubtype.IsNone() || Item.Subtype == GearSubtype) &&
			(GearRarity.IsNone() || Item.Rarity == GearRarity) && (!bGearUsableOnly || Item.bUsable) &&
			Item.LocationId != TEXT("WeaponSocket") && Item.LocationId != TEXT("SkillSocket"))
		{
			Rows.Add(&Item);
		}
	}
	Rows.Sort([this](const FGolemGearItemRow& A, const FGolemGearItemRow& B)
	{
		if (bGearSortByItemLevel && A.ItemLevel != B.ItemLevel)
		{
			return A.ItemLevel > B.ItemLevel;
		}
		return A.Name.CompareTo(B.Name) < 0;
	});
	AddLine(Bag, NSLOCTEXT("GolemGearUI", "OwnedBag", "OWNED ITEMS · assignments retain their owner"), 14);
	for (const FGolemGearItemRow* Item : Rows)
	{
		UGolemRPGActionButton* Button = MakeButton(FText::Format(NSLOCTEXT("GolemGearUI", "BagItem",
			"{0} [{8}] · {1} · iLvl {2} · ReqLv {3} · +{4} · ×{5} · #{9}\n{6} {7}"), Item->Name,
			FText::FromName(Item->Rarity), Item->ItemLevel > 0 ? FText::AsNumber(Item->ItemLevel) :
			NSLOCTEXT("GolemGearUI", "NoItemLevel", "None"), FText::AsNumber(Item->RequiredLevel),
			FText::AsNumber(Item->UpgradeRank), FText::AsNumber(Item->Count), Item->Location, Item->Requirement,
			Item->Badge, FText::FromString(Item->InstanceId.ToString().Left(8))), Item->Tooltip);
		Button->SetCompact(true);
		Button->SetCaptionColor(Item->QualityColor);
		Button->SetPresentationState(!bSelectingVendorStock && Item->InstanceId == SelectedGearInstanceId, false);
		const FGuid ItemId = Item->InstanceId;
		Button->OnClicked().AddWeakLambda(this, [this, ItemId]()
		{
			SelectGearItem(ItemId, false);
		});
		Bag->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 3.0f));
		Buttons.Add(Button);
	}
	if (Rows.IsEmpty())
	{
		AddLine(Bag, NSLOCTEXT("GolemGearUI", "FilteredEmpty", "No owned items match these filters."), 13);
	}
	if (Snapshot.Gear.bHasDemoState)
	{
		AddLine(Bag, NSLOCTEXT("GolemGearUI", "VendorTitle", "SANCTUARY VENDOR · fixture stock, real costs"), 15);
	}
	for (const FGolemGearItemRow& Item : Snapshot.Gear.VendorStock)
	{
		if (Item.Category != GearCategory)
		{
			continue;
		}
		UGolemRPGActionButton* Stock = MakeButton(FText::Format(NSLOCTEXT("GolemGearUI", "Stock",
			"Buy {0} · {1} currency each"), Item.Name, FText::AsNumber(Item.BuyPrice)), Item.Tooltip);
		Stock->SetCompact(true);
		const FPrimaryAssetId Definition = Item.DefinitionId;
		Stock->OnClicked().AddWeakLambda(this, [this, Definition]()
		{
			SelectedVendorDefinition = Definition;
			SelectGearItem(FGuid(), true);
		});
		Bag->AddChildToVerticalBox(Stock)->SetPadding(FMargin(0.0f, 2.0f));
		Buttons.Add(Stock);
	}
	const FGolemGearItemRow* Selected = FindSelectedGearItem();
	if (Selected == nullptr)
	{
		AddLine(Inspect, NSLOCTEXT("GolemGearUI", "SelectItem", "Select an owned item or vendor stock to inspect and stage a command."), 15);
		return;
	}
	AddLine(Inspect, FText::Format(NSLOCTEXT("GolemGearUI", "SelectedItemSummary", "{0}\n{1} · {2}"),
		Selected->Name, Selected->Location, Selected->Requirement), 13)->SetColorAndOpacity(FSlateColor(Selected->QualityColor));
	AddLine(Inspect, NSLOCTEXT("GolemGearUI", "CompareTarget", "COMPARE / EQUIP TARGET · choose Ring1 or Ring2 explicitly"), 14);
	GearTargetSelector = WidgetTree->ConstructWidget<UComboBoxString>();
	GearTargetSelector->AddOption(TEXT("Choose target slot"));
	for (const FGolemGearSlotRow& GearSlot : Snapshot.Gear.Slots)
	{
		GearTargetSelector->AddOption(GearSlot.Name.ToString());
		if (GearSlot.SlotId == GearTargetSlot)
		{
			GearTargetSelector->SetSelectedOption(GearSlot.Name.ToString());
		}
	}
	if (GearTargetSlot.IsNone())
	{
		GearTargetSelector->SetSelectedOption(TEXT("Choose target slot"));
	}
	GearTargetSelector->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleGearTargetSelected);
	GearTargetSelector->SetIsEnabled(!bPending);
	Inspect->AddChildToVerticalBox(GearTargetSelector);
	if (Selected->Subtype == TEXT("Ring"))
	{
		UHorizontalBox* Rings = WidgetTree->ConstructWidget<UHorizontalBox>();
		Inspect->AddChildToVerticalBox(Rings);
		for (int32 RingIndex = 1; RingIndex <= 2; ++RingIndex)
		{
			const FName RingSlot(*FString::Printf(TEXT("Equipment.Slot.Ring%d"), RingIndex));
			UGolemRPGActionButton* Ring = MakeButton(FText::Format(
				NSLOCTEXT("GolemGearUI", "RingTarget", "Compare Ring{0}"), FText::AsNumber(RingIndex)));
			Ring->SetCompact(true);
			Ring->SetPresentationState(GearTargetSlot == RingSlot, false);
			Ring->SetIsEnabled(!bPending);
			Ring->OnClicked().AddWeakLambda(this, [this, RingSlot]()
			{
				GearTargetSlot = RingSlot;
				RequestGearCompare();
				RebuildActivePage();
				RefreshPresentation();
			});
			Rings->AddChildToHorizontalBox(Ring)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Buttons.Add(Ring);
		}
	}
	if (!GearCompare.Stats.IsEmpty())
	{
		FText CompleteComparison = FText::Format(NSLOCTEXT("GolemGearUI", "CompleteComparison",
			"COMPLETE COMPARISON\n{0}\n{1}"), GearCompare.Summary, GearCompare.RejectionReason);
		for (const FGolemGearStatRow& Row : GearCompare.Stats)
		{
			CompleteComparison = FText::Format(NSLOCTEXT("GolemGearUI", "CompleteComparisonRow",
				"{0}\n{1}: {2} → {3}\n{4}"), CompleteComparison, Row.Name, Row.Before, Row.After, Row.Contribution);
		}
		UGolemRPGActionButton* Read = MakeButton(NSLOCTEXT("GolemGearUI", "ReadComparison",
			"Read complete comparison [LT / RT]"));
		Read->SetCompact(true);
		Read->OnInspectRequested.AddWeakLambda(this, [this, CompleteComparison]()
		{
			ShowDetail(CompleteComparison);
		});
		Read->OnClicked().AddWeakLambda(this, [this, CompleteComparison]()
		{
			ShowDetail(CompleteComparison);
		});
		Inspect->AddChildToVerticalBox(Read);
		Buttons.Add(Read);
	}
	AddLine(Inspect, GearCompare.RejectionReason, 13);
	for (const FGolemGearStatRow& Row : GearCompare.Stats)
	{
		AddLine(Inspect, FText::Format(NSLOCTEXT("GolemGearUI", "CompareStat", "{0}: {1} → {2}\n{3}"),
			Row.Name, Row.Before, Row.After, Row.Contribution), 12);
	}
	AddLine(Inspect, GearCompare.Summary, 13);
	AddLine(Inspect, Selected->Tooltip, 13)->SetColorAndOpacity(FSlateColor(Selected->QualityColor));
	const EGolemGearPanelAction Actions[] = {EGolemGearPanelAction::Equip, EGolemGearPanelAction::Unequip,
		EGolemGearPanelAction::SwapRings,
		EGolemGearPanelAction::Transfer, EGolemGearPanelAction::Upgrade, EGolemGearPanelAction::Buy,
		EGolemGearPanelAction::Sell, EGolemGearPanelAction::AttachWeaponRune, EGolemGearPanelAction::DetachWeaponRune,
		EGolemGearPanelAction::AttachSkillRune, EGolemGearPanelAction::DetachSkillRune};
	const FText ActionCaptions[] = {NSLOCTEXT("GolemGearUI", "Equip", "Stage equip to selected slot"),
		NSLOCTEXT("GolemGearUI", "Unequip", "Stage unequip"), NSLOCTEXT("GolemGearUI", "SwapRings", "Stage swap Ring1 ↔ Ring2"),
		NSLOCTEXT("GolemGearUI", "Transfer", "Stage transfer to selected Golem"),
		NSLOCTEXT("GolemGearUI", "Upgrade", "Stage upgrade +1 · 20 currency +4 M01 +2 M02 +1 M03"),
		NSLOCTEXT("GolemGearUI", "BuyOne", "Stage buy one"), NSLOCTEXT("GolemGearUI", "SellOne", "Stage sell one · no buyback"),
		NSLOCTEXT("GolemGearUI", "AttachWeapon", "Stage attach Rune to this Golem's weapon"),
		NSLOCTEXT("GolemGearUI", "DetachWeapon", "Stage detach this weapon's Rune"),
		NSLOCTEXT("GolemGearUI", "AttachSkill", "Stage attach skill Rune"),
		NSLOCTEXT("GolemGearUI", "DetachSkill", "Stage detach skill Rune")};
	GearTransferSelector = WidgetTree->ConstructWidget<UComboBoxString>();
	GearTransferSelector->AddOption(TEXT("Choose transfer target"));
	for (const FGolemGearGolemRow& Golem : Snapshot.Gear.Golems)
	{
		GearTransferSelector->AddOption(Golem.Name.ToString());
		if (Golem.GolemId == GearTransferTarget)
		{
			GearTransferSelector->SetSelectedOption(Golem.Name.ToString());
		}
	}
	GearTransferSelector->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleGearTransferSelected);
	GearTransferSelector->SetIsEnabled(!bPending);
	Inspect->AddChildToVerticalBox(GearTransferSelector);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Actions); ++Index)
	{
		const EGolemGearPanelAction Action = Actions[Index];
		const bool bEquipment = Selected->Category == TEXT("Equipment");
		const bool bWeapon = Selected->Subtype == TEXT("Hammer");
		const bool bRune = Selected->Category == TEXT("AttachRune");
		bool bRelevant = Action == EGolemGearPanelAction::Buy ? bSelectingVendorStock : !bSelectingVendorStock;
		if (Action == EGolemGearPanelAction::Equip || Action == EGolemGearPanelAction::Unequip || Action == EGolemGearPanelAction::Transfer)
		{
			bRelevant &= bEquipment;
		}
		if (Action == EGolemGearPanelAction::Upgrade)
		{
			bRelevant &= Selected->bCanUpgrade;
		}
		if (Action == EGolemGearPanelAction::Unequip)
		{
			bRelevant &= Selected->LocationId == TEXT("Equipment") && Selected->AssignedGolemId == Snapshot.Gear.GolemId;
		}
		if (Action == EGolemGearPanelAction::SwapRings)
		{
			bRelevant &= Selected->Subtype == TEXT("Ring") && Selected->AssignedGolemId == Snapshot.Gear.GolemId;
		}
		if (Action == EGolemGearPanelAction::Transfer)
		{
			bRelevant &= Selected->LocationId == TEXT("Equipment");
		}
		if (Action == EGolemGearPanelAction::AttachWeaponRune)
		{
			bRelevant &= bRune && Selected->RuneId == TEXT("Weapon.Aftershock");
		}
		if (Action == EGolemGearPanelAction::DetachWeaponRune)
		{
			bRelevant &= bWeapon && Selected->AttachedRuneId.IsValid();
		}
		if (Action == EGolemGearPanelAction::AttachSkillRune)
		{
			bRelevant &= bRune && Selected->RuneId != TEXT("Weapon.Aftershock");
		}
		if (Action == EGolemGearPanelAction::DetachSkillRune)
		{
			bRelevant &= Snapshot.Gear.SkillRuneInstanceId.IsValid();
		}
		if (Action == EGolemGearPanelAction::Sell)
		{
			bRelevant &= Selected->SellPrice > 0 && Selected->LocationId == TEXT("Inventory");
		}
		if (!bRelevant)
		{
			continue;
		}
		UGolemRPGActionButton* Button = MakeButton(ActionCaptions[Index]);
		Button->SetCompact(true);
		Button->SetIsEnabled(!bPending && Snapshot.Gear.bCanEdit);
		Button->OnClicked().AddWeakLambda(this, [this, Action]()
		{
			StageGearAction(Action);
		});
		Inspect->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 3.0f));
		Buttons.Add(Button);
	}
	if (Selected->Category == TEXT("Consumable"))
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			UGolemRPGActionButton* Bind = MakeButton(FText::Format(NSLOCTEXT("GolemGearUI", "BindQuick", "Stage bind Q{0}"), FText::AsNumber(Index + 1)));
			Bind->SetIsEnabled(!bPending && Snapshot.Gear.bCanEdit);
			Bind->OnClicked().AddWeakLambda(this, [this, Index]()
			{
				StageGearAction(EGolemGearPanelAction::BindConsumable);
				GearDraft.QuickSlot = Index;
			});
			Inspect->AddChildToVerticalBox(Bind);
			Buttons.Add(Bind);
		}
	}
	if (Selected->Rarity == TEXT("Epic") && !bSelectingVendorStock)
	{
		UCheckBox* Confirm = WidgetTree->ConstructWidget<UCheckBox>();
		UTextBlock* Warning = WidgetTree->ConstructWidget<UTextBlock>();
		Warning->SetText(NSLOCTEXT("GolemGearUI", "EpicSale", "Confirm Epic sale: permanent, no buyback"));
		Confirm->SetContent(Warning);
		Confirm->SetIsChecked(bConfirmEpicSale);
		Confirm->OnCheckStateChanged.AddDynamic(this, &ThisClass::HandleGearEpicConfirmed);
		Inspect->AddChildToVerticalBox(Confirm);
	}
	AddLine(Inspect, Snapshot.Gear.EditLockReason, 13);
}

void UGolemRPGPanel::SelectGearItem(FGuid InstanceId, bool bVendorStock)
{
	if (bPending)
	{
		return;
	}
	ResetGearDraft();
	SelectedGearInstanceId = InstanceId;
	bSelectingVendorStock = bVendorStock;
	const FGolemGearItemRow* Item = FindSelectedGearItem();
	if (Item != nullptr)
	{
		if (Item->Subtype == TEXT("Ring") && GearTargetSlot != TEXT("Equipment.Slot.Ring1") && GearTargetSlot != TEXT("Equipment.Slot.Ring2"))
		{
			GearTargetSlot = NAME_None;
		}
		else if (Item->Subtype != TEXT("Ring") && Item->Category == TEXT("Equipment"))
		{
			GearTargetSlot = FName(*FString::Printf(TEXT("Equipment.Slot.%s"),
				Item->Subtype == TEXT("Hammer") ? TEXT("Weapon") : *Item->Subtype.ToString()));
		}
		ShowDetail(Item->Tooltip);
	}
	RequestGearCompare();
	RebuildActivePage();
	RefreshPresentation();
}

void UGolemRPGPanel::RequestGearCompare()
{
	GearCompare = FGolemGearCompare();
	FGolemGearPanelIntent Query;
	Query.GolemId = Snapshot.Gear.GolemId;
	Query.InstanceId = SelectedGearInstanceId;
	Query.SlotId = GearTargetSlot;
	Query.Action = bGearDraft ? GearDraft.Action : EGolemGearPanelAction::Equip;
	Query.TargetGolemId = bGearDraft ? GearDraft.TargetGolemId : GearTransferTarget;
	OnGearPreviewRequested.Broadcast(Query);
}

void UGolemRPGPanel::StageGearAction(EGolemGearPanelAction Action)
{
	const FGolemGearItemRow* Item = FindSelectedGearItem();
	if (Item == nullptr || bPending)
	{
		return;
	}
	GearDraft = FGolemGearPanelIntent();
	GearDraft.Action = Action;
	GearDraft.GolemId = Snapshot.Gear.GolemId;
	GearDraft.InstanceId = Item->InstanceId;
	GearDraft.DefinitionId = Item->DefinitionId;
	GearDraft.SlotId = GearTargetSlot;
	GearDraft.ExpectedInventoryRevision = Snapshot.Gear.InventoryRevision;
	GearDraft.ExpectedGolemRevision = Snapshot.Gear.GolemRevision;
	GearDraft.ExpectedItemRevision = Item->ItemRevision;
	GearDraft.bConfirmEpicSale = bConfirmEpicSale;
	bHasDraft = false;
	Draft = Snapshot.Build;
	if (Action == EGolemGearPanelAction::Transfer)
	{
		GearDraft.GolemId = Item->AssignedGolemId;
		GearDraft.TargetGolemId = GearTransferTarget;
		for (const FGolemGearGolemRow& Golem : Snapshot.Gear.Golems)
		{
			if (Golem.GolemId == Item->AssignedGolemId)
			{
				GearDraft.ExpectedGolemRevision = Golem.Revision;
			}
			if (Golem.GolemId == GearTransferTarget)
			{
				GearDraft.ExpectedTargetGolemRevision = Golem.Revision;
			}
		}
	}
	if (Action == EGolemGearPanelAction::AttachWeaponRune)
	{
		GearDraft.RuneInstanceId = Item->InstanceId;
		const FGolemGearSlotRow* Weapon = GolemGearUI::FindSlot(Snapshot.Gear, TEXT("Equipment.Slot.Weapon"));
		GearDraft.InstanceId = Weapon != nullptr ? Weapon->InstanceId : FGuid();
		const FGolemGearItemRow* Parent = Snapshot.Gear.Items.FindByPredicate([this](const FGolemGearItemRow& Row)
		{
			return Row.InstanceId == GearDraft.InstanceId;
		});
		GearDraft.ExpectedItemRevision = Parent != nullptr ? Parent->ItemRevision : 0;
	}
	if (Action == EGolemGearPanelAction::Equip && GearTargetSlot.IsNone())
	{
		ShowDetail(NSLOCTEXT("GolemGearUI", "ExplicitTargetRequired", "Choose the target slot. Ring1 and Ring2 are explicit independent targets."));
		return;
	}
	bGearDraft = true;
	RequestGearCompare();
	RebuildActivePage();
	RefreshPresentation();
}

void UGolemRPGPanel::SubmitGearCommand()
{
	if (!bGearDraft || bPending || !OnGearCommandRequested.IsBound())
	{
		return;
	}
	GearDraft.RequestId = FGuid::NewGuid();
	PendingRequestId = GearDraft.RequestId;
	bPending = true;
	RebuildActivePage();
	RefreshPresentation();
	OnGearCommandRequested.Broadcast(GearDraft);
}

void UGolemRPGPanel::ResetGearDraft()
{
	bGearDraft = false;
	GearDraft = FGolemGearPanelIntent();
	GearCompare = FGolemGearCompare();
	bConfirmEpicSale = false;
}

void UGolemRPGPanel::RequestGearFixture(FName Action)
{
	const bool bDomainFixture = Action == TEXT("Guard") || Action == TEXT("Mend") ||
		Action == TEXT("Strike") || Action == TEXT("ClaimReward");
	if (bPending || (bDomainFixture ? !Snapshot.Gear.bCanEdit : !Snapshot.Gear.bCanRunFixtures))
	{
		return;
	}
	FGolemGearPanelIntent Intent;
	Intent.Action = EGolemGearPanelAction::Fixture;
	Intent.GolemId = Snapshot.Gear.GolemId;
	Intent.RequestId = FGuid::NewGuid();
	Intent.FixtureAction = Action;
	Intent.RewardId = Snapshot.Gear.ClaimableRewardId;
	Intent.ExpectedInventoryRevision = Snapshot.Gear.InventoryRevision;
	Intent.ExpectedGolemRevision = Snapshot.Gear.GolemRevision;
	PendingRequestId = Intent.RequestId;
	bPending = true;
	OnGearCommandRequested.Broadcast(Intent);
	RefreshPresentation();
}

void UGolemRPGPanel::HandleGearGolemSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bRebuildingPage || bPending)
	{
		return;
	}
	for (const FGolemGearGolemRow& Golem : Snapshot.Gear.Golems)
	{
		if (Golem.Name.ToString() != SelectedItem)
		{
			continue;
		}
		FGolemGearPanelIntent Intent;
		Intent.Action = EGolemGearPanelAction::SelectGolem;
		Intent.GolemId = Golem.GolemId;
		OnGearCommandRequested.Broadcast(Intent);
		return;
	}
}

void UGolemRPGPanel::HandleGearTransferSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bRebuildingPage || bPending)
	{
		return;
	}
	for (const FGolemGearGolemRow& Golem : Snapshot.Gear.Golems)
	{
		if (Golem.Name.ToString() == SelectedItem)
		{
			GearTransferTarget = Golem.GolemId;
			if (bGearDraft && GearDraft.Action == EGolemGearPanelAction::Transfer)
			{
				ResetGearDraft();
				RequestGearCompare();
				RefreshPresentation();
				ShowDetail(NSLOCTEXT("GolemGearUI", "RestageTransfer", "Transfer target changed. Stage the transfer again to review its exact participants."));
			}
			return;
		}
	}
}

void UGolemRPGPanel::HandleGearTargetSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bRebuildingPage || bPending)
	{
		return;
	}
	for (const FGolemGearSlotRow& GearSlot : Snapshot.Gear.Slots)
	{
		if (GearSlot.Name.ToString() == SelectedItem)
		{
			GearTargetSlot = GearSlot.SlotId;
			RequestGearCompare();
			RebuildActivePage();
			return;
		}
	}
}

void UGolemRPGPanel::HandleGearSubtypeSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	GearSubtype = SelectedItem == TEXT("All subtypes") ? NAME_None : FName(*SelectedItem);
	RebuildActivePage();
}

void UGolemRPGPanel::HandleGearRaritySelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	GearRarity = SelectedItem == TEXT("All rarities") ? NAME_None : FName(*SelectedItem);
	RebuildActivePage();
}

void UGolemRPGPanel::HandleGearSortSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	bGearSortByItemLevel = SelectedItem == TEXT("Sort iLvl");
	RebuildActivePage();
}

void UGolemRPGPanel::HandleGearUsableChanged(bool bChecked)
{
	bGearUsableOnly = bChecked;
	RebuildActivePage();
}

void UGolemRPGPanel::HandleGearEpicConfirmed(bool bChecked)
{
	bConfirmEpicSale = bChecked;
	if (bGearDraft && GearDraft.Action == EGolemGearPanelAction::Sell)
	{
		GearDraft.bConfirmEpicSale = bChecked;
	}
}
