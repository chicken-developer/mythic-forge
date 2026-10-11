#include "Widgets/KnightRPGPanel.h"

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
#include "Widgets/KnightRPGActionButton.h"

namespace KnightGearUI
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

	const FKnightGearSlotRow* FindSlot(const FKnightGearPanelSnapshot& Gear, FName SlotId)
	{
		return Gear.Slots.FindByPredicate([SlotId](const FKnightGearSlotRow& GearSlot)
		{
			return GearSlot.SlotId == SlotId;
		});
	}
}

const FKnightGearCompare& UKnightRPGPanel::GetGearCompare() const
{
	return GearCompare;
}

const FKnightGearPanelIntent& UKnightRPGPanel::GetGearDraft() const
{
	return GearDraft;
}

FGuid UKnightRPGPanel::GetSelectedGearInstanceId() const
{
	return SelectedGearInstanceId;
}

bool UKnightRPGPanel::HasGearDraft() const
{
	return bGearDraft;
}

void UKnightRPGPanel::SetGearCompare(const FKnightGearCompare& InCompare)
{
	if (InCompare.KnightId == Snapshot.Gear.KnightId && InCompare.InstanceId == SelectedGearInstanceId &&
		InCompare.SlotId == GearTargetSlot)
	{
		GearCompare = InCompare;
	}
}

void UKnightRPGPanel::AddGearSlot(UVerticalBox* Parent, const FKnightGearSlotRow& GearSlot)
{
	const FText ItemLabel = GearSlot.InstanceId.IsValid() ? GearSlot.ItemName :
		NSLOCTEXT("KnightGearUI", "EmptySlot", "Empty");
	UKnightRPGActionButton* Button = MakeButton(FText::Format(
		NSLOCTEXT("KnightGearUI", "SlotCaption", "{0}\n{1} {2}"), GearSlot.Name,
		GearSlot.Badge.IsEmpty() ? FText() : FText::Format(NSLOCTEXT("KnightGearUI", "Badge", "[{0}]"), GearSlot.Badge), ItemLabel), GearSlot.Tooltip);
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

void UKnightRPGPanel::BuildGearCharacter(UVerticalBox* Parent)
{
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Columns);
	UVerticalBox* Doll = KnightGearUI::Column(WidgetTree, Columns, 1.15f);
	UVerticalBox* Stats = KnightGearUI::ScrollColumn(WidgetTree, Columns, 0.85f);
	UComboBoxString* KnightSelector = WidgetTree->ConstructWidget<UComboBoxString>();
	for (const FKnightGearKnightRow& Knight : Snapshot.Gear.Knights)
	{
		KnightSelector->AddOption(Knight.Name.ToString());
		if (Knight.KnightId == Snapshot.Gear.KnightId)
		{
			KnightSelector->SetSelectedOption(Knight.Name.ToString());
		}
	}
	KnightSelector->SetIsEnabled(!bPending);
	KnightSelector->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleGearKnightSelected);
	Doll->AddChildToVerticalBox(KnightSelector);
	AddLine(Doll, FText::Format(NSLOCTEXT("KnightGearUI", "GearLevel", "Level {0} · 12 equipment slots"),
		FText::AsNumber(Snapshot.Gear.Level)), 13);
	UHorizontalBox* PaperDoll = WidgetTree->ConstructWidget<UHorizontalBox>();
	Doll->AddChildToVerticalBox(PaperDoll);
	UVerticalBox* Left = KnightGearUI::Column(WidgetTree, PaperDoll, 1.0f);
	UVerticalBox* Model = KnightGearUI::Column(WidgetTree, PaperDoll, 1.35f);
	UVerticalBox* Right = KnightGearUI::Column(WidgetTree, PaperDoll, 1.0f);
	const FName LeftSlots[] = {TEXT("Equipment.Slot.Head"), TEXT("Equipment.Slot.Shoulders"),
		TEXT("Equipment.Slot.Chest"), TEXT("Equipment.Slot.Wrists"), TEXT("Equipment.Slot.Hands"), TEXT("Equipment.Slot.Waist")};
	const FName RightSlots[] = {TEXT("Equipment.Slot.Feet"), TEXT("Equipment.Slot.Neck"),
		TEXT("Equipment.Slot.Ring1"), TEXT("Equipment.Slot.Ring2"), TEXT("Equipment.Slot.Relic")};
	for (FName SlotId : LeftSlots)
	{
		if (const FKnightGearSlotRow* GearSlot = KnightGearUI::FindSlot(Snapshot.Gear, SlotId))
		{
			AddGearSlot(Left, *GearSlot);
		}
	}
	for (FName SlotId : RightSlots)
	{
		if (const FKnightGearSlotRow* GearSlot = KnightGearUI::FindSlot(Snapshot.Gear, SlotId))
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
	if (const FKnightGearSlotRow* Weapon = KnightGearUI::FindSlot(Snapshot.Gear, TEXT("Equipment.Slot.Weapon")))
	{
		AddGearSlot(Model, *Weapon);
	}
	UKnightRPGActionButton* Bag = MakeButton(NSLOCTEXT("KnightGearUI", "OpenBag", "Open shared inventory / compare"));
	Bag->OnClicked().AddWeakLambda(this, [this]()
	{
		SetActiveTab(EKnightRPGPanelTab::Inventory);
	});
	Doll->AddChildToVerticalBox(Bag);
	Buttons.Add(Bag);
	AddLine(Doll, Snapshot.Gear.EditLockReason, 12);
	AddLine(Stats, NSLOCTEXT("KnightGearUI", "Totals", "ATTRIBUTES · confirmed totals"), 18);
	for (const FKnightRPGStatRow& Row : Snapshot.StatRows)
	{
		UKnightRPGActionButton* Value = MakeButton(FText::Format(
			NSLOCTEXT("KnightGearUI", "Stat", "{0}: {1}"), Row.Name, Row.Value), Row.Contributions);
		Value->SetCompact(true);
		Stats->AddChildToVerticalBox(Value);
		Buttons.Add(Value);
	}
	AddLine(Stats, NSLOCTEXT("KnightGearUI", "SlotRule", "Rings are separate targets. Rune sockets and Q1/Q2 are outside the 12 equipment slots."), 12);
	AddLine(Stats, Snapshot.Gear.RelicStatus, 12);
	AddLine(Stats, Snapshot.Gear.Attachments, 12);
	for (int32 Index = 0; Index < Snapshot.Gear.QuickUses.Num(); ++Index)
	{
		const FKnightGearQuickUseRow& Quick = Snapshot.Gear.QuickUses[Index];
		AddLine(Stats, FText::Format(NSLOCTEXT("KnightGearUI", "QuickBinding", "Q{0}: {1} ×{2} · shared potion cooldown30s"),
			FText::AsNumber(Index + 1), Quick.Name, FText::AsNumber(Quick.Count)), 12);
	}
	BuildGearFixtureControls(Parent);
}

void UKnightRPGPanel::BuildGearFixtureControls(UVerticalBox* Parent)
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
		NSLOCTEXT("KnightGearUI", "NewDemo", "Fixture: New Demo Run"),
		NSLOCTEXT("KnightGearUI", "CatalogLab", "DEV ONLY: Catalog Lab"),
		NSLOCTEXT("KnightGearUI", "ResetCatalog", "DEV ONLY: Reset Catalog"),
		NSLOCTEXT("KnightGearUI", "Recovery", "Fixture: Sanctuary Recovery")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Actions); ++Index)
	{
		UKnightRPGActionButton* Button = MakeButton(Labels[Index]);
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
	const FText PresetLabels[] = {NSLOCTEXT("KnightGearUI", "GuardPreset", "DEV ONLY preset: Guard"),
		NSLOCTEXT("KnightGearUI", "MendPreset", "DEV ONLY preset: Mend"),
		NSLOCTEXT("KnightGearUI", "StrikePreset", "DEV ONLY preset: Strike"),
		NSLOCTEXT("KnightGearUI", "ClaimReward", "Claim confirmed encounter reward")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(PresetActions); ++Index)
	{
		UKnightRPGActionButton* Button = MakeButton(PresetLabels[Index]);
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
	UKnightRPGActionButton* SnapshotButton = MakeButton(NSLOCTEXT("KnightGearUI", "Reconstruct",
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

const FKnightGearItemRow* UKnightRPGPanel::FindSelectedGearItem() const
{
	if (bSelectingVendorStock)
	{
		return Snapshot.Gear.VendorStock.FindByPredicate([this](const FKnightGearItemRow& Item)
		{
			return Item.DefinitionId == SelectedVendorDefinition;
		});
	}
	return Snapshot.Gear.Items.FindByPredicate([this](const FKnightGearItemRow& Item)
	{
		return Item.InstanceId == SelectedGearInstanceId;
	});
}

void UKnightRPGPanel::BuildGearInventory(UVerticalBox* Parent)
{
	AddLine(Parent, FText::Format(NSLOCTEXT("KnightGearUI", "GearBagHeading", "SHARED INVENTORY · inspect {0}, Lv{1} · currency {2}"),
		Snapshot.Gear.KnightName, FText::AsNumber(Snapshot.Gear.Level), FText::AsNumber(Snapshot.Gear.Currency)), 18);
	BuildGearFixtureControls(Parent);
	UHorizontalBox* Categories = WidgetTree->ConstructWidget<UHorizontalBox>();
	Parent->AddChildToVerticalBox(Categories);
	const FName CategoryIds[] = {TEXT("Equipment"), TEXT("AttachRune"), TEXT("Consumable"), TEXT("Material"), TEXT("QuestItem"), TEXT("Junk")};
	const FText Captions[] = {NSLOCTEXT("KnightGearUI", "Equipment", "Equipment"), NSLOCTEXT("KnightGearUI", "Runes", "Runes"),
		NSLOCTEXT("KnightGearUI", "Consumables", "Consumables"), NSLOCTEXT("KnightGearUI", "Materials", "Materials"),
		NSLOCTEXT("KnightGearUI", "Quest", "Quest"), NSLOCTEXT("KnightGearUI", "Junk", "Junk")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CategoryIds); ++Index)
	{
		UKnightRPGActionButton* Button = MakeButton(Captions[Index]);
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
	for (const FKnightGearItemRow& Item : Snapshot.Gear.Items)
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
	UsableLabel->SetText(NSLOCTEXT("KnightGearUI", "Usable", "Usable by selected Golem"));
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
	UVerticalBox* Bag = KnightGearUI::ScrollColumn(WidgetTree, Content, 1.0f);
	UVerticalBox* Inspect = KnightGearUI::ScrollColumn(WidgetTree, Content, 1.15f);
	TArray<const FKnightGearItemRow*> Rows;
	for (const FKnightGearItemRow& Item : Snapshot.Gear.Items)
	{
		if (Item.Category == GearCategory && (GearSubtype.IsNone() || Item.Subtype == GearSubtype) &&
			(GearRarity.IsNone() || Item.Rarity == GearRarity) && (!bGearUsableOnly || Item.bUsable) &&
			Item.LocationId != TEXT("WeaponSocket") && Item.LocationId != TEXT("SkillSocket"))
		{
			Rows.Add(&Item);
		}
	}
	Rows.Sort([this](const FKnightGearItemRow& A, const FKnightGearItemRow& B)
	{
		if (bGearSortByItemLevel && A.ItemLevel != B.ItemLevel)
		{
			return A.ItemLevel > B.ItemLevel;
		}
		return A.Name.CompareTo(B.Name) < 0;
	});
	AddLine(Bag, NSLOCTEXT("KnightGearUI", "OwnedBag", "OWNED ITEMS · assignments retain their owner"), 14);
	for (const FKnightGearItemRow* Item : Rows)
	{
		UKnightRPGActionButton* Button = MakeButton(FText::Format(NSLOCTEXT("KnightGearUI", "BagItem",
			"{0} [{8}] · {1} · iLvl {2} · ReqLv {3} · +{4} · ×{5} · #{9}\n{6} {7}"), Item->Name,
			FText::FromName(Item->Rarity), Item->ItemLevel > 0 ? FText::AsNumber(Item->ItemLevel) :
			NSLOCTEXT("KnightGearUI", "NoItemLevel", "None"), FText::AsNumber(Item->RequiredLevel),
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
		AddLine(Bag, NSLOCTEXT("KnightGearUI", "FilteredEmpty", "No owned items match these filters."), 13);
	}
	if (Snapshot.Gear.bHasDemoState)
	{
		AddLine(Bag, NSLOCTEXT("KnightGearUI", "VendorTitle", "SANCTUARY VENDOR · fixture stock, real costs"), 15);
	}
	for (const FKnightGearItemRow& Item : Snapshot.Gear.VendorStock)
	{
		if (Item.Category != GearCategory)
		{
			continue;
		}
		UKnightRPGActionButton* Stock = MakeButton(FText::Format(NSLOCTEXT("KnightGearUI", "Stock",
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
	const FKnightGearItemRow* Selected = FindSelectedGearItem();
	if (Selected == nullptr)
	{
		AddLine(Inspect, NSLOCTEXT("KnightGearUI", "SelectItem", "Select an owned item or vendor stock to inspect and stage a command."), 15);
		return;
	}
	AddLine(Inspect, FText::Format(NSLOCTEXT("KnightGearUI", "SelectedItemSummary", "{0}\n{1} · {2}"),
		Selected->Name, Selected->Location, Selected->Requirement), 13)->SetColorAndOpacity(FSlateColor(Selected->QualityColor));
	AddLine(Inspect, NSLOCTEXT("KnightGearUI", "CompareTarget", "COMPARE / EQUIP TARGET · choose Ring1 or Ring2 explicitly"), 14);
	GearTargetSelector = WidgetTree->ConstructWidget<UComboBoxString>();
	GearTargetSelector->AddOption(TEXT("Choose target slot"));
	for (const FKnightGearSlotRow& GearSlot : Snapshot.Gear.Slots)
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
			UKnightRPGActionButton* Ring = MakeButton(FText::Format(
				NSLOCTEXT("KnightGearUI", "RingTarget", "Compare Ring{0}"), FText::AsNumber(RingIndex)));
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
		FText CompleteComparison = FText::Format(NSLOCTEXT("KnightGearUI", "CompleteComparison",
			"COMPLETE COMPARISON\n{0}\n{1}"), GearCompare.Summary, GearCompare.RejectionReason);
		for (const FKnightGearStatRow& Row : GearCompare.Stats)
		{
			CompleteComparison = FText::Format(NSLOCTEXT("KnightGearUI", "CompleteComparisonRow",
				"{0}\n{1}: {2} → {3}\n{4}"), CompleteComparison, Row.Name, Row.Before, Row.After, Row.Contribution);
		}
		UKnightRPGActionButton* Read = MakeButton(NSLOCTEXT("KnightGearUI", "ReadComparison",
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
	for (const FKnightGearStatRow& Row : GearCompare.Stats)
	{
		AddLine(Inspect, FText::Format(NSLOCTEXT("KnightGearUI", "CompareStat", "{0}: {1} → {2}\n{3}"),
			Row.Name, Row.Before, Row.After, Row.Contribution), 12);
	}
	AddLine(Inspect, GearCompare.Summary, 13);
	AddLine(Inspect, Selected->Tooltip, 13)->SetColorAndOpacity(FSlateColor(Selected->QualityColor));
	const EKnightGearPanelAction Actions[] = {EKnightGearPanelAction::Equip, EKnightGearPanelAction::Unequip,
		EKnightGearPanelAction::SwapRings,
		EKnightGearPanelAction::Transfer, EKnightGearPanelAction::Upgrade, EKnightGearPanelAction::Buy,
		EKnightGearPanelAction::Sell, EKnightGearPanelAction::AttachWeaponRune, EKnightGearPanelAction::DetachWeaponRune,
		EKnightGearPanelAction::AttachSkillRune, EKnightGearPanelAction::DetachSkillRune};
	const FText ActionCaptions[] = {NSLOCTEXT("KnightGearUI", "Equip", "Stage equip to selected slot"),
		NSLOCTEXT("KnightGearUI", "Unequip", "Stage unequip"), NSLOCTEXT("KnightGearUI", "SwapRings", "Stage swap Ring1 ↔ Ring2"),
		NSLOCTEXT("KnightGearUI", "Transfer", "Stage transfer to selected Golem"),
		NSLOCTEXT("KnightGearUI", "Upgrade", "Stage upgrade +1 · 20 currency +4 M01 +2 M02 +1 M03"),
		NSLOCTEXT("KnightGearUI", "BuyOne", "Stage buy one"), NSLOCTEXT("KnightGearUI", "SellOne", "Stage sell one · no buyback"),
		NSLOCTEXT("KnightGearUI", "AttachWeapon", "Stage attach Rune to this Golem's weapon"),
		NSLOCTEXT("KnightGearUI", "DetachWeapon", "Stage detach this weapon's Rune"),
		NSLOCTEXT("KnightGearUI", "AttachSkill", "Stage attach skill Rune"),
		NSLOCTEXT("KnightGearUI", "DetachSkill", "Stage detach skill Rune")};
	GearTransferSelector = WidgetTree->ConstructWidget<UComboBoxString>();
	GearTransferSelector->AddOption(TEXT("Choose transfer target"));
	for (const FKnightGearKnightRow& Knight : Snapshot.Gear.Knights)
	{
		GearTransferSelector->AddOption(Knight.Name.ToString());
		if (Knight.KnightId == GearTransferTarget)
		{
			GearTransferSelector->SetSelectedOption(Knight.Name.ToString());
		}
	}
	GearTransferSelector->OnSelectionChanged.AddDynamic(this, &ThisClass::HandleGearTransferSelected);
	GearTransferSelector->SetIsEnabled(!bPending);
	Inspect->AddChildToVerticalBox(GearTransferSelector);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Actions); ++Index)
	{
		const EKnightGearPanelAction Action = Actions[Index];
		const bool bEquipment = Selected->Category == TEXT("Equipment");
		const bool bWeapon = Selected->Subtype == TEXT("Hammer");
		const bool bRune = Selected->Category == TEXT("AttachRune");
		bool bRelevant = Action == EKnightGearPanelAction::Buy ? bSelectingVendorStock : !bSelectingVendorStock;
		if (Action == EKnightGearPanelAction::Equip || Action == EKnightGearPanelAction::Unequip || Action == EKnightGearPanelAction::Transfer)
		{
			bRelevant &= bEquipment;
		}
		if (Action == EKnightGearPanelAction::Upgrade)
		{
			bRelevant &= Selected->bCanUpgrade;
		}
		if (Action == EKnightGearPanelAction::Unequip)
		{
			bRelevant &= Selected->LocationId == TEXT("Equipment") && Selected->AssignedKnightId == Snapshot.Gear.KnightId;
		}
		if (Action == EKnightGearPanelAction::SwapRings)
		{
			bRelevant &= Selected->Subtype == TEXT("Ring") && Selected->AssignedKnightId == Snapshot.Gear.KnightId;
		}
		if (Action == EKnightGearPanelAction::Transfer)
		{
			bRelevant &= Selected->LocationId == TEXT("Equipment");
		}
		if (Action == EKnightGearPanelAction::AttachWeaponRune)
		{
			bRelevant &= bRune && Selected->RuneId == TEXT("Weapon.Aftershock");
		}
		if (Action == EKnightGearPanelAction::DetachWeaponRune)
		{
			bRelevant &= bWeapon && Selected->AttachedRuneId.IsValid();
		}
		if (Action == EKnightGearPanelAction::AttachSkillRune)
		{
			bRelevant &= bRune && Selected->RuneId != TEXT("Weapon.Aftershock");
		}
		if (Action == EKnightGearPanelAction::DetachSkillRune)
		{
			bRelevant &= Snapshot.Gear.SkillRuneInstanceId.IsValid();
		}
		if (Action == EKnightGearPanelAction::Sell)
		{
			bRelevant &= Selected->SellPrice > 0 && Selected->LocationId == TEXT("Inventory");
		}
		if (!bRelevant)
		{
			continue;
		}
		UKnightRPGActionButton* Button = MakeButton(ActionCaptions[Index]);
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
			UKnightRPGActionButton* Bind = MakeButton(FText::Format(NSLOCTEXT("KnightGearUI", "BindQuick", "Stage bind Q{0}"), FText::AsNumber(Index + 1)));
			Bind->SetIsEnabled(!bPending && Snapshot.Gear.bCanEdit);
			Bind->OnClicked().AddWeakLambda(this, [this, Index]()
			{
				StageGearAction(EKnightGearPanelAction::BindConsumable);
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
		Warning->SetText(NSLOCTEXT("KnightGearUI", "EpicSale", "Confirm Epic sale: permanent, no buyback"));
		Confirm->SetContent(Warning);
		Confirm->SetIsChecked(bConfirmEpicSale);
		Confirm->OnCheckStateChanged.AddDynamic(this, &ThisClass::HandleGearEpicConfirmed);
		Inspect->AddChildToVerticalBox(Confirm);
	}
	AddLine(Inspect, Snapshot.Gear.EditLockReason, 13);
}

void UKnightRPGPanel::SelectGearItem(FGuid InstanceId, bool bVendorStock)
{
	if (bPending)
	{
		return;
	}
	ResetGearDraft();
	SelectedGearInstanceId = InstanceId;
	bSelectingVendorStock = bVendorStock;
	const FKnightGearItemRow* Item = FindSelectedGearItem();
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

void UKnightRPGPanel::RequestGearCompare()
{
	GearCompare = FKnightGearCompare();
	FKnightGearPanelIntent Query;
	Query.KnightId = Snapshot.Gear.KnightId;
	Query.InstanceId = SelectedGearInstanceId;
	Query.SlotId = GearTargetSlot;
	Query.Action = bGearDraft ? GearDraft.Action : EKnightGearPanelAction::Equip;
	Query.TargetKnightId = bGearDraft ? GearDraft.TargetKnightId : GearTransferTarget;
	OnGearPreviewRequested.Broadcast(Query);
}

void UKnightRPGPanel::StageGearAction(EKnightGearPanelAction Action)
{
	const FKnightGearItemRow* Item = FindSelectedGearItem();
	if (Item == nullptr || bPending)
	{
		return;
	}
	GearDraft = FKnightGearPanelIntent();
	GearDraft.Action = Action;
	GearDraft.KnightId = Snapshot.Gear.KnightId;
	GearDraft.InstanceId = Item->InstanceId;
	GearDraft.DefinitionId = Item->DefinitionId;
	GearDraft.SlotId = GearTargetSlot;
	GearDraft.ExpectedInventoryRevision = Snapshot.Gear.InventoryRevision;
	GearDraft.ExpectedKnightRevision = Snapshot.Gear.KnightRevision;
	GearDraft.ExpectedItemRevision = Item->ItemRevision;
	GearDraft.bConfirmEpicSale = bConfirmEpicSale;
	bHasDraft = false;
	Draft = Snapshot.Build;
	if (Action == EKnightGearPanelAction::Transfer)
	{
		GearDraft.KnightId = Item->AssignedKnightId;
		GearDraft.TargetKnightId = GearTransferTarget;
		for (const FKnightGearKnightRow& Knight : Snapshot.Gear.Knights)
		{
			if (Knight.KnightId == Item->AssignedKnightId)
			{
				GearDraft.ExpectedKnightRevision = Knight.Revision;
			}
			if (Knight.KnightId == GearTransferTarget)
			{
				GearDraft.ExpectedTargetKnightRevision = Knight.Revision;
			}
		}
	}
	if (Action == EKnightGearPanelAction::AttachWeaponRune)
	{
		GearDraft.RuneInstanceId = Item->InstanceId;
		const FKnightGearSlotRow* Weapon = KnightGearUI::FindSlot(Snapshot.Gear, TEXT("Equipment.Slot.Weapon"));
		GearDraft.InstanceId = Weapon != nullptr ? Weapon->InstanceId : FGuid();
		const FKnightGearItemRow* Parent = Snapshot.Gear.Items.FindByPredicate([this](const FKnightGearItemRow& Row)
		{
			return Row.InstanceId == GearDraft.InstanceId;
		});
		GearDraft.ExpectedItemRevision = Parent != nullptr ? Parent->ItemRevision : 0;
	}
	if (Action == EKnightGearPanelAction::Equip && GearTargetSlot.IsNone())
	{
		ShowDetail(NSLOCTEXT("KnightGearUI", "ExplicitTargetRequired", "Choose the target slot. Ring1 and Ring2 are explicit independent targets."));
		return;
	}
	bGearDraft = true;
	RequestGearCompare();
	RebuildActivePage();
	RefreshPresentation();
}

void UKnightRPGPanel::SubmitGearCommand()
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

void UKnightRPGPanel::ResetGearDraft()
{
	bGearDraft = false;
	GearDraft = FKnightGearPanelIntent();
	GearCompare = FKnightGearCompare();
	bConfirmEpicSale = false;
}

void UKnightRPGPanel::RequestGearFixture(FName Action)
{
	const bool bDomainFixture = Action == TEXT("Guard") || Action == TEXT("Mend") ||
		Action == TEXT("Strike") || Action == TEXT("ClaimReward");
	if (bPending || (bDomainFixture ? !Snapshot.Gear.bCanEdit : !Snapshot.Gear.bCanRunFixtures))
	{
		return;
	}
	FKnightGearPanelIntent Intent;
	Intent.Action = EKnightGearPanelAction::Fixture;
	Intent.KnightId = Snapshot.Gear.KnightId;
	Intent.RequestId = FGuid::NewGuid();
	Intent.FixtureAction = Action;
	Intent.RewardId = Snapshot.Gear.ClaimableRewardId;
	Intent.ExpectedInventoryRevision = Snapshot.Gear.InventoryRevision;
	Intent.ExpectedKnightRevision = Snapshot.Gear.KnightRevision;
	PendingRequestId = Intent.RequestId;
	bPending = true;
	OnGearCommandRequested.Broadcast(Intent);
	RefreshPresentation();
}

void UKnightRPGPanel::HandleGearKnightSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bRebuildingPage || bPending)
	{
		return;
	}
	for (const FKnightGearKnightRow& Knight : Snapshot.Gear.Knights)
	{
		if (Knight.Name.ToString() != SelectedItem)
		{
			continue;
		}
		FKnightGearPanelIntent Intent;
		Intent.Action = EKnightGearPanelAction::SelectKnight;
		Intent.KnightId = Knight.KnightId;
		OnGearCommandRequested.Broadcast(Intent);
		return;
	}
}

void UKnightRPGPanel::HandleGearTransferSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bRebuildingPage || bPending)
	{
		return;
	}
	for (const FKnightGearKnightRow& Knight : Snapshot.Gear.Knights)
	{
		if (Knight.Name.ToString() == SelectedItem)
		{
			GearTransferTarget = Knight.KnightId;
			if (bGearDraft && GearDraft.Action == EKnightGearPanelAction::Transfer)
			{
				ResetGearDraft();
				RequestGearCompare();
				RefreshPresentation();
				ShowDetail(NSLOCTEXT("KnightGearUI", "RestageTransfer", "Transfer target changed. Stage the transfer again to review its exact participants."));
			}
			return;
		}
	}
}

void UKnightRPGPanel::HandleGearTargetSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (bRebuildingPage || bPending)
	{
		return;
	}
	for (const FKnightGearSlotRow& GearSlot : Snapshot.Gear.Slots)
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

void UKnightRPGPanel::HandleGearSubtypeSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	GearSubtype = SelectedItem == TEXT("All subtypes") ? NAME_None : FName(*SelectedItem);
	RebuildActivePage();
}

void UKnightRPGPanel::HandleGearRaritySelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	GearRarity = SelectedItem == TEXT("All rarities") ? NAME_None : FName(*SelectedItem);
	RebuildActivePage();
}

void UKnightRPGPanel::HandleGearSortSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	bGearSortByItemLevel = SelectedItem == TEXT("Sort iLvl");
	RebuildActivePage();
}

void UKnightRPGPanel::HandleGearUsableChanged(bool bChecked)
{
	bGearUsableOnly = bChecked;
	RebuildActivePage();
}

void UKnightRPGPanel::HandleGearEpicConfirmed(bool bChecked)
{
	bConfirmEpicSale = bChecked;
	if (bGearDraft && GearDraft.Action == EKnightGearPanelAction::Sell)
	{
		GearDraft.bConfirmEpicSale = bChecked;
	}
}
