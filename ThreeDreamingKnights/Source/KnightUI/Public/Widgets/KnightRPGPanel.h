#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Components/ComboBoxString.h"
#include "Models/KnightRPGPanelTypes.h"
#include "Models/KnightCampaignPanelTypes.h"
#include "KnightRPGPanel.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnKnightStoryPanelAction, FName);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnKnightStoryChoiceAction, FName, FName);

struct KNIGHTUI_API FKnightStoryChoicePresentation
{
	FName AnchorId;
	FName ChoiceId;
	FText Label;
};

class UKnightRPGActionButton;
class UTextBlock;
class USizeBox;
class UVerticalBox;
class UHorizontalBox;
class UCanvasPanel;
class UEditableTextBox;
class UImage;
class UScrollBox;
class UCheckBox;
class UTextureRenderTarget2D;
class USceneCaptureComponent2D;

/** A real CommonUI input layer with native content; actions are intents consumed by the player composition. */
UCLASS()
class KNIGHTUI_API UKnightRPGPanel : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UKnightRPGPanel();
	void SetSnapshot(const FKnightRPGPanelSnapshot& InSnapshot);
	void SetStoryPresentation(const FText& Text, bool bHost, bool bPending);
	FOnKnightStoryPanelAction OnStoryActionRequested;
	void SetStoryChoices(const TArray<FKnightStoryChoicePresentation>& Choices, bool bPending);
	FOnKnightStoryChoiceAction OnStoryChoiceRequested;
	void SetCampaignPresentation(const FKnightCampaignPanelSnapshot& InSnapshot);
	void UpdateMapPosition(const FVector2D& UV, bool bVisible);
	const FKnightCampaignPanelSnapshot& GetCampaignPresentation() const;
	FOnKnightCampaignPanelAction OnCampaignActionRequested;
	FOnKnightCampaignKeyRemap OnCampaignKeyRemapRequested;
	FSimpleMulticastDelegate OnPanelClosed;
	void SetBuildPreview(const FKnightRPGBuildPreview& InPreview);
	void SetGearCompare(const FKnightGearCompare& InCompare);
	void SetActiveTab(EKnightRPGPanelTab InTab);
	EKnightRPGPanelTab GetActiveTab() const;
	const FKnightRPGPanelSnapshot& GetSnapshot() const;
	const FKnightSkillBuild& GetDraftBuild() const;
	const FKnightGearCompare& GetGearCompare() const;
	const FKnightGearPanelIntent& GetGearDraft() const;
	FGuid GetSelectedGearInstanceId() const;
	bool HasGearDraft() const;
	bool HasPendingCommand() const;
	FGuid GetPendingRequestId() const;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	FOnKnightRPGPanelAction OnActionRequested;
	FOnKnightRPGPanelCommand OnCommandRequested;
	FOnKnightRPGPanelPreview OnPreviewRequested;
	FOnKnightGearPanelCommand OnGearCommandRequested;
	FOnKnightGearPanelPreview OnGearPreviewRequested;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnHandleBackAction() override;

private:
	friend class FKnightRPGPanelStateTest;
	friend class FKnightGearPanelSelectionTest;
	friend class FKnightRPGUIClosedReceiptTest;
	UTextBlock* AddLine(UVerticalBox* Parent, const FText& Text, int32 FontSize = 16);
	UKnightRPGActionButton* MakeButton(const FText& Caption, const FText& Tooltip = FText());
	void AddAction(UVerticalBox* Parent, EKnightRPGPanelAction Action, const FText& Caption);
	void RefreshPresentation();
	void RebuildActivePage();
	void BuildCharacter(UVerticalBox* Parent);
	void BuildSpecialization(UVerticalBox* Parent);
	void BuildTalents(UVerticalBox* Parent);
	void BuildSpellbook(UVerticalBox* Parent);
	void BuildRunes(UVerticalBox* Parent);
	void BuildGearCharacter(UVerticalBox* Parent);
	void BuildGearInventory(UVerticalBox* Parent);
	void BuildCampaign(UVerticalBox* Parent);
	void BuildJournal(UVerticalBox* Parent);
	void BuildCampaignMap(UVerticalBox* Parent);
	void BuildSettings(UVerticalBox* Parent);
	void AddCampaignAction(UVerticalBox* Parent, const FText& Caption, FName Action,
		FName Context = NAME_None, bool bEnabled = true);
	void ApplyCampaignTextScale();
	void BuildGearFixtureControls(UVerticalBox* Parent);
	void AddGearSlot(UVerticalBox* Parent, const FKnightGearSlotRow& GearSlot);
	void SelectGearItem(FGuid InstanceId, bool bVendorStock);
	void RequestGearCompare();
	void StageGearAction(EKnightGearPanelAction Action);
	void SubmitGearCommand();
	void ResetGearDraft();
	void RequestGearFixture(FName Action);
	const FKnightGearItemRow* FindSelectedGearItem() const;
	UFUNCTION()
	void HandleGearKnightSelected(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleGearTransferSelected(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleGearTargetSelected(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleGearSubtypeSelected(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleGearRaritySelected(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleGearSortSelected(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleGearUsableChanged(bool bChecked);
	UFUNCTION()
	void HandleGearEpicConfirmed(bool bChecked);
	void BuildTalentTree(UHorizontalBox* Parent, bool bClassTree);
	void AddTreeNode(UCanvasPanel* Canvas, const FVector2D& Position, uint8 Shape,
		const FText& Name, const FText& Tooltip, bool bChoice, bool bClassTree, const FText& UnderLabel = FText());
	void StageChoice(bool bClassTree, FName Choice);
	void StageSpec(FName SpecId);
	void StageRune(const FKnightRPGRuneRow& Rune);
	void RevertDraft();
	void RequestPreview();
	void SubmitCommand(EKnightRPGPanelAction Action);
	void ShowDetail(const FText& Text);
	void ScrollControlIntoView(UWidget* Control) const;
	void DeferPageFocus(UWidget* Control);
	void HandlePageLayoutFinished(float DeltaTime);
	void CancelDeferredPageFocus();
	void CreatePaperDollPreview();
	void ReleasePaperDollPreview();
	const FKnightRPGSpecRow* FindDraftSpec() const;
	bool CanStage() const;
	bool IsDraftDifferent() const;
	UFUNCTION()
	void HandleLoadoutSelected(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION()
	void HandleKnightSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeaderText;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> StoryBody;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StoryText;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UKnightRPGActionButton>> StoryButtons;
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> StoryChoiceBody;
	TArray<FKnightStoryChoicePresentation> DisplayedStoryChoices;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ObjectiveText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailText;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> DetailScroll;
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> CampaignDetailContainer;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CampaignDetailHelp;
	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> GearTargetSelector;
	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> GearTransferSelector;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> PageBody;
	UPROPERTY(Transient)
	TObjectPtr<UImage> PaperDollImage;
	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> LoadoutSelector;
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> LoadoutName;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UKnightRPGActionButton>> StatValueButtons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UKnightRPGActionButton>> Buttons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UKnightRPGActionButton>> TabButtons;
	UPROPERTY(Transient)
	TObjectPtr<UKnightRPGActionButton> ApplyButton;
	UPROPERTY(Transient)
	TObjectPtr<UKnightRPGActionButton> ContinueButton;
	UPROPERTY(Transient)
	TObjectPtr<UKnightRPGActionButton> ClassChoiceFirstButton;
	UPROPERTY(Transient)
	TObjectPtr<UKnightRPGActionButton> SpecChoiceFirstButton;
	UPROPERTY(Transient)
	TObjectPtr<AActor> PreviewActor;
	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponent2D> PreviewCapture;
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PreviewTarget;
	FKnightRPGPanelSnapshot Snapshot;
	FKnightRPGBuildPreview Preview;
	FKnightSkillBuild Draft;
	int64 DraftRevision = 0;
	FGuid DraftWeaponRuneInstanceId;
	FGuid DraftSkillRuneInstanceId;
	FGuid SelectedLoadoutId;
	FGuid PendingRequestId;
	EKnightRPGPanelTab ActiveTab = EKnightRPGPanelTab::Character;
	FName SpellCategory = TEXT("Spec");
	FName SelectedSpellId;
	bool bHasDraft = false;
	bool bPending = false;
	bool bClassChoiceFocused = true;
	bool bRebuildingPage = false;
	bool bShowClassChoices = false;
	bool bShowSpecChoices = false;
	FKnightGearPanelIntent GearDraft;
	FKnightGearCompare GearCompare;
	FGuid SelectedGearInstanceId;
	FPrimaryAssetId SelectedVendorDefinition;
	FName GearCategory = TEXT("Equipment");
	FName GearSubtype;
	FName GearRarity;
	FName GearTargetSlot = TEXT("Equipment.Slot.Weapon");
	FKnightEntityId GearTransferTarget;
	bool bGearDraft = false;
	bool bGearSortByItemLevel = false;
	bool bGearUsableOnly = false;
	bool bSelectingVendorStock = false;
	bool bConfirmEpicSale = false;
	TWeakObjectPtr<UWidget> DeferredPageFocus;
	TWeakPtr<SWidget> DeferredOriginalFocus;
	FDelegateHandle PageLayoutHandle;
	int32 PageLayoutPasses = 0;
	FKnightCampaignPanelSnapshot CampaignSnapshot;
	FName CapturingBinding;
	FName SelectedMapMarker;
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> CampaignNavigation;
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> PanelFooter;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UKnightRPGActionButton>> CampaignTabButtons;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MapPlayerLabel;
	TMap<TWeakObjectPtr<UTextBlock>, int32> OriginalFontSizes;
};
