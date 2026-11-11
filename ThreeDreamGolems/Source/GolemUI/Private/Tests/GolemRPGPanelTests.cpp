#include "Widgets/GolemRPGPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemRPGPanelStateTest,
	"Golem.M55.UI.DraftPendingStaleAndIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemRPGPanelStateTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGolemRPGPanel> Panel(NewObject<UGolemRPGPanel>());
	FGolemRPGPanelSnapshot Confirmed;
	Confirmed.GolemId = FGolemEntityId::Create();
	Confirmed.Build.SpecId = TEXT("Uranus");
	Confirmed.Revision = 41;
	Confirmed.AvailablePoints = 2;
	Confirmed.bCanEditBuild = true;
	Panel->SetSnapshot(Confirmed);
	int32 PreviewCount = 0;
	TArray<FGolemRPGPanelCommand> Submitted;
	const FDelegateHandle PreviewHandle = Panel->OnPreviewRequested.AddLambda(
		[&PreviewCount](const FGolemSkillBuild&)
		{
			++PreviewCount;
		});
	const FDelegateHandle CommandHandle = Panel->OnCommandRequested.AddLambda(
		[&Submitted](const FGolemRPGPanelCommand& Command)
		{
			Submitted.Add(Command);
		});
	Panel->StageChoice(true, TEXT("Reservoir"));
	Panel->StageChoice(false, TEXT("A"));
	TestTrue(TEXT("Staging never rewrites confirmed build"), Panel->GetSnapshot().Build == Confirmed.Build);
	TestEqual(TEXT("Staging only requests pure previews"), PreviewCount, 2);
	TestEqual(TEXT("No authoritative command while staging"), Submitted.Num(), 0);
	Confirmed.Revision = 42;
	Confirmed.AvailablePoints = 3;
	Panel->SetSnapshot(Confirmed);
	TestEqual(TEXT("External revision does not silently rebase user draft"), Panel->DraftRevision, int64(41));
	Panel->SubmitCommand(EGolemRPGPanelAction::ApplyBuild);
	TestEqual(TEXT("Exactly one command is submitted"), Submitted.Num(), 1);
	if (Submitted.Num() != 1)
	{
		Panel->OnPreviewRequested.Remove(PreviewHandle);
		Panel->OnCommandRequested.Remove(CommandHandle);
		return false;
	}
	TestEqual(TEXT("Stale draft carries original expected revision"), Submitted[0].ExpectedRevision, int64(41));
	TestTrue(TEXT("Pending request is identifiable"), Submitted[0].RequestId.IsValid());
	Panel->SubmitCommand(EGolemRPGPanelAction::ApplyBuild);
	Panel->StageSpec(TEXT("Venus"));
	TestEqual(TEXT("Double-submit and edits are blocked while pending"), Submitted.Num(), 1);
	TestEqual(TEXT("Pending edit does not change the submitted specialization"), Panel->Draft.SpecId, FName(TEXT("Uranus")));
	TestFalse(TEXT("Native receipt-order fixture represents a deactivated panel"), Panel->IsActivated());
	TestTrue(TEXT("The pending identity remains available while the panel is closed"),
		Panel->GetPendingRequestId() == Submitted[0].RequestId);
	Confirmed.LastCompletedRequestId = FGuid::NewGuid();
	Panel->SetSnapshot(Confirmed);
	TestTrue(TEXT("An unrelated closed receipt cannot consume the exact pending request"),
		Panel->HasPendingCommand() && Panel->GetPendingRequestId() == Submitted[0].RequestId);
	Confirmed.LastCompletedRequestId = Submitted[0].RequestId;
	Confirmed.Feedback = NSLOCTEXT("GolemRPGUITest", "Stale", "Rejected: stale revision");
	Panel->SetSnapshot(Confirmed);
	TestFalse(TEXT("Explicit response releases pending"), Panel->HasPendingCommand());
	TestTrue(TEXT("Rejected command refreshes draft from confirmed state"), Panel->GetDraftBuild() == Confirmed.Build);
	TestEqual(TEXT("Rejected mutation is never automatically retried"), Submitted.Num(), 1);
	Confirmed.LastCompletedRequestId = FGuid::NewGuid();
	Panel->SetSnapshot(Confirmed);
	TestFalse(TEXT("A later gameplay receipt cannot restore a consumed closed-panel Pending state"),
		Panel->HasPendingCommand() || Panel->GetPendingRequestId().IsValid());
	Panel->StageChoice(true, TEXT("Endurance"));
	FGolemRPGPanelSnapshot Other = Confirmed;
	Other.GolemId = FGolemEntityId::Create();
	Other.Build.SpecId = TEXT("Neptune");
	Other.LastCompletedRequestId.Invalidate();
	Panel->SetSnapshot(Other);
	TestTrue(TEXT("Golem change discards previous Golem's staged choices"), Panel->GetDraftBuild() == Other.Build);
	FGolemRPGBuildPreview LatePreview;
	LatePreview.GolemId = Confirmed.GolemId;
	LatePreview.Build = Confirmed.Build;
	LatePreview.Summary = NSLOCTEXT("GolemRPGUITest", "OldPreview", "Old Golem preview");
	Panel->SetBuildPreview(LatePreview);
	TestTrue(TEXT("Late preview from previous build is ignored"), Panel->Preview.Build == Other.Build);

	// A selected preset and populated rune/name controls must not contaminate unrelated command payloads.
	const FGuid LoadoutId = FGuid::NewGuid();
	Other.ActiveTalentLoadoutId = LoadoutId;
	Panel->LoadoutName = NewObject<UEditableTextBox>(Panel.Get());
	Panel->LoadoutName->SetText(NSLOCTEXT("GolemRPGUITest", "PresetName", "  Test loadout  "));
	const EGolemRPGPanelAction PresetActions[]
	{
		EGolemRPGPanelAction::NewTalentLoadout,
		EGolemRPGPanelAction::RenameTalentLoadout,
		EGolemRPGPanelAction::DuplicateTalentLoadout,
		EGolemRPGPanelAction::DeleteTalentLoadout,
		EGolemRPGPanelAction::ApplyTalentLoadout,
		EGolemRPGPanelAction::ApplyBuild
	};
	for (EGolemRPGPanelAction Action : PresetActions)
	{
		++Other.Revision;
		Panel->SetSnapshot(Other);
		Panel->StageChoice(true, TEXT("Reservoir"));
		FGolemRPGRuneRow Weapon;
		Weapon.RuneId = TEXT("Weapon.Aftershock");
		Weapon.ItemInstanceId = FGuid::NewGuid();
		Weapon.bWeaponSocket = true;
		Weapon.bCompatible = true;
		Panel->StageRune(Weapon);
		FGolemRPGRuneRow Skill;
		Skill.RuneId = TEXT("Skill.Convergence");
		Skill.ItemInstanceId = FGuid::NewGuid();
		Skill.bCompatible = true;
		Panel->StageRune(Skill);
		const FGolemSkillBuild BeforeSubmit = Panel->GetDraftBuild();
		const int32 PreviousCount = Submitted.Num();
		Panel->SubmitCommand(Action);
		if (!TestEqual(TEXT("Each valid preset/build intent emits exactly one request"), Submitted.Num(), PreviousCount + 1))
		{
			break;
		}
		const FGolemRPGPanelCommand& Intent = Submitted.Last();
		TestTrue(TEXT("Constructing a command preserves the user's staged build"), Panel->GetDraftBuild() == BeforeSubmit);
		if (Action == EGolemRPGPanelAction::ApplyBuild)
		{
			TestTrue(TEXT("Apply carries exact staged build and both rune item identities"),
				Intent.ProposedBuild == BeforeSubmit && Intent.WeaponRuneInstanceId == Weapon.ItemInstanceId
				&& Intent.SkillRuneInstanceId == Skill.ItemInstanceId);
			TestFalse(TEXT("Selected preset does not contaminate ApplyBuild"), Intent.TalentLoadoutId.IsValid());
			TestTrue(TEXT("Name entry does not contaminate ApplyBuild"), Intent.TalentLoadoutName.IsEmpty());
		}
		else
		{
			TestFalse(TEXT("Preset CRUD does not transmit weapon attachment identity"), Intent.WeaponRuneInstanceId.IsValid());
			TestFalse(TEXT("Preset CRUD does not transmit skill attachment identity"), Intent.SkillRuneInstanceId.IsValid());
			if (Action == EGolemRPGPanelAction::NewTalentLoadout)
			{
				TestEqual(TEXT("New preset contains intended specialization"), Intent.ProposedBuild.SpecId, BeforeSubmit.SpecId);
				TestEqual(TEXT("New preset contains staged talents"), Intent.ProposedBuild.ClassChoice, BeforeSubmit.ClassChoice);
				TestTrue(TEXT("New preset excludes both attachment keys"),
					Intent.ProposedBuild.WeaponRune.IsNone() && Intent.ProposedBuild.SkillRune.IsNone());
				TestFalse(TEXT("New preset does not target previously selected preset"), Intent.TalentLoadoutId.IsValid());
			}
			else
			{
				TestTrue(TEXT("Existing preset operation transmits only its identity"),
					Intent.ProposedBuild == FGolemSkillBuild() && Intent.TalentLoadoutId == LoadoutId);
			}
			const bool bNamed = Action == EGolemRPGPanelAction::NewTalentLoadout
				|| Action == EGolemRPGPanelAction::RenameTalentLoadout
				|| Action == EGolemRPGPanelAction::DuplicateTalentLoadout;
			TestEqual(TEXT("Only name-writing preset operations carry trimmed name"),
				Intent.TalentLoadoutName, bNamed ? FString(TEXT("Test loadout")) : FString());
		}
		Other.LastCompletedRequestId = Intent.RequestId;
		Panel->SetSnapshot(Other);
	}
	Panel->OnPreviewRequested.Remove(PreviewHandle);
	Panel->OnCommandRequested.Remove(CommandHandle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemGearPanelSelectionTest,
	"Golem.M56.UI.InspectionAndTransferSelections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemGearPanelSelectionTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGolemRPGPanel> Panel(NewObject<UGolemRPGPanel>());
	FGolemRPGPanelSnapshot Confirmed;
	Confirmed.GolemId = FGolemEntityId::Create();
	Confirmed.Gear.GolemId = Confirmed.GolemId;
	Confirmed.Gear.bEnabled = true;
	Confirmed.Gear.bCanEdit = true;
	Confirmed.Gear.InventoryRevision = 10;
	const TCHAR* Names[] = {TEXT("Sky"), TEXT("Sea"), TEXT("Earth")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		FGolemGearGolemRow Golem;
		Golem.GolemId = Index == 0 ? Confirmed.GolemId : FGolemEntityId::Create();
		Golem.Name = FText::FromString(Names[Index]);
		Golem.Revision = 20 + Index;
		Confirmed.Gear.Golems.Add(Golem);
	}
	FGolemGearItemRow Weapon;
	Weapon.InstanceId = FGuid::NewGuid();
	Weapon.AssignedGolemId = Confirmed.GolemId;
	Weapon.Category = TEXT("Equipment");
	Weapon.Subtype = TEXT("Hammer");
	Weapon.LocationId = TEXT("Equipment");
	Weapon.ItemRevision = 4;
	Confirmed.Gear.Items.Add(Weapon);
	Panel->SetSnapshot(Confirmed);
	TArray<FGolemGearPanelIntent> Submitted;
	const FDelegateHandle Handle = Panel->OnGearCommandRequested.AddLambda(
		[&Submitted](const FGolemGearPanelIntent& Intent)
		{
			Submitted.Add(Intent);
		});
	TStrongObjectPtr<UComboBoxString> Inspection(NewObject<UComboBoxString>(Panel.Get()));
	Panel->GearTransferSelector = NewObject<UComboBoxString>(Panel.Get());
	for (const TCHAR* Name : Names)
	{
		Inspection->AddOption(Name);
		Panel->GearTransferSelector->AddOption(Name);
	}
	Inspection->OnSelectionChanged.AddDynamic(Panel.Get(), &UGolemRPGPanel::HandleGearGolemSelected);
	Panel->GearTransferSelector->OnSelectionChanged.AddDynamic(Panel.Get(), &UGolemRPGPanel::HandleGearTransferSelected);
	Panel->GearTransferSelector->SetSelectedOption(TEXT("Sea"));
	TestEqual(TEXT("Selecting transfer participant sends no domain or inspection command"), Submitted.Num(), 0);
	TestTrue(TEXT("Transfer destination is the exact selected canonical Golem"),
		Panel->GearTransferTarget == Confirmed.Gear.Golems[1].GolemId);
	Inspection->SetSelectedOption(TEXT("Sea"));
	TestEqual(TEXT("Inspecting the same name as transfer target still publishes a view selection"), Submitted.Num(), 1);
	if (Submitted.Num() == 1)
	{
		TestTrue(TEXT("Inspection has the correct view-only action and exact identity"),
			Submitted[0].Action == EGolemGearPanelAction::SelectGolem &&
			Submitted[0].GolemId == Confirmed.Gear.Golems[1].GolemId && !Submitted[0].RequestId.IsValid());
	}
	Panel->SelectGearItem(Weapon.InstanceId, false);
	Panel->StageGearAction(EGolemGearPanelAction::Transfer);
	TestTrue(TEXT("Transfer draft retains exact source and destination revisions"), Panel->HasGearDraft() &&
		Panel->GetGearDraft().GolemId == Confirmed.GolemId &&
		Panel->GetGearDraft().TargetGolemId == Confirmed.Gear.Golems[1].GolemId &&
		Panel->GetGearDraft().ExpectedGolemRevision == 20 &&
		Panel->GetGearDraft().ExpectedTargetGolemRevision == 21);
	Panel->GearTransferSelector->SetSelectedOption(TEXT("Earth"));
	TestFalse(TEXT("Changing the destination invalidates the reviewed transfer draft"), Panel->HasGearDraft());
	TestTrue(TEXT("Changed destination is exact and never changes confirmed ownership"),
		Panel->GearTransferTarget == Confirmed.Gear.Golems[2].GolemId &&
		Panel->GetSnapshot().Gear.Items[0].AssignedGolemId == Confirmed.GolemId);
	TestEqual(TEXT("Target changes do not submit or automatically retry authority mutations"), Submitted.Num(), 1);
	Panel->OnGearCommandRequested.Remove(Handle);
	return true;
}

#endif
