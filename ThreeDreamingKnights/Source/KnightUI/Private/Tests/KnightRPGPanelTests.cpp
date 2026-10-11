#include "Widgets/KnightRPGPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightRPGPanelStateTest,
	"Knight.M55.UI.DraftPendingStaleAndIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightRPGPanelStateTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UKnightRPGPanel> Panel(NewObject<UKnightRPGPanel>());
	FKnightRPGPanelSnapshot Confirmed;
	Confirmed.KnightId = FKnightEntityId::Create();
	Confirmed.Build.SpecId = TEXT("Uranus");
	Confirmed.Revision = 41;
	Confirmed.AvailablePoints = 2;
	Confirmed.bCanEditBuild = true;
	Panel->SetSnapshot(Confirmed);
	int32 PreviewCount = 0;
	TArray<FKnightRPGPanelCommand> Submitted;
	const FDelegateHandle PreviewHandle = Panel->OnPreviewRequested.AddLambda(
		[&PreviewCount](const FKnightSkillBuild&)
		{
			++PreviewCount;
		});
	const FDelegateHandle CommandHandle = Panel->OnCommandRequested.AddLambda(
		[&Submitted](const FKnightRPGPanelCommand& Command)
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
	Panel->SubmitCommand(EKnightRPGPanelAction::ApplyBuild);
	TestEqual(TEXT("Exactly one command is submitted"), Submitted.Num(), 1);
	if (Submitted.Num() != 1)
	{
		Panel->OnPreviewRequested.Remove(PreviewHandle);
		Panel->OnCommandRequested.Remove(CommandHandle);
		return false;
	}
	TestEqual(TEXT("Stale draft carries original expected revision"), Submitted[0].ExpectedRevision, int64(41));
	TestTrue(TEXT("Pending request is identifiable"), Submitted[0].RequestId.IsValid());
	Panel->SubmitCommand(EKnightRPGPanelAction::ApplyBuild);
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
	Confirmed.Feedback = NSLOCTEXT("KnightRPGUITest", "Stale", "Rejected: stale revision");
	Panel->SetSnapshot(Confirmed);
	TestFalse(TEXT("Explicit response releases pending"), Panel->HasPendingCommand());
	TestTrue(TEXT("Rejected command refreshes draft from confirmed state"), Panel->GetDraftBuild() == Confirmed.Build);
	TestEqual(TEXT("Rejected mutation is never automatically retried"), Submitted.Num(), 1);
	Confirmed.LastCompletedRequestId = FGuid::NewGuid();
	Panel->SetSnapshot(Confirmed);
	TestFalse(TEXT("A later gameplay receipt cannot restore a consumed closed-panel Pending state"),
		Panel->HasPendingCommand() || Panel->GetPendingRequestId().IsValid());
	Panel->StageChoice(true, TEXT("Endurance"));
	FKnightRPGPanelSnapshot Other = Confirmed;
	Other.KnightId = FKnightEntityId::Create();
	Other.Build.SpecId = TEXT("Neptune");
	Other.LastCompletedRequestId.Invalidate();
	Panel->SetSnapshot(Other);
	TestTrue(TEXT("Knight change discards previous Knight's staged choices"), Panel->GetDraftBuild() == Other.Build);
	FKnightRPGBuildPreview LatePreview;
	LatePreview.KnightId = Confirmed.KnightId;
	LatePreview.Build = Confirmed.Build;
	LatePreview.Summary = NSLOCTEXT("KnightRPGUITest", "OldPreview", "Old Knight preview");
	Panel->SetBuildPreview(LatePreview);
	TestTrue(TEXT("Late preview from previous build is ignored"), Panel->Preview.Build == Other.Build);

	// A selected preset and populated rune/name controls must not contaminate unrelated command payloads.
	const FGuid LoadoutId = FGuid::NewGuid();
	Other.ActiveTalentLoadoutId = LoadoutId;
	Panel->LoadoutName = NewObject<UEditableTextBox>(Panel.Get());
	Panel->LoadoutName->SetText(NSLOCTEXT("KnightRPGUITest", "PresetName", "  Test loadout  "));
	const EKnightRPGPanelAction PresetActions[]
	{
		EKnightRPGPanelAction::NewTalentLoadout,
		EKnightRPGPanelAction::RenameTalentLoadout,
		EKnightRPGPanelAction::DuplicateTalentLoadout,
		EKnightRPGPanelAction::DeleteTalentLoadout,
		EKnightRPGPanelAction::ApplyTalentLoadout,
		EKnightRPGPanelAction::ApplyBuild
	};
	for (EKnightRPGPanelAction Action : PresetActions)
	{
		++Other.Revision;
		Panel->SetSnapshot(Other);
		Panel->StageChoice(true, TEXT("Reservoir"));
		FKnightRPGRuneRow Weapon;
		Weapon.RuneId = TEXT("Weapon.Aftershock");
		Weapon.ItemInstanceId = FGuid::NewGuid();
		Weapon.bWeaponSocket = true;
		Weapon.bCompatible = true;
		Panel->StageRune(Weapon);
		FKnightRPGRuneRow Skill;
		Skill.RuneId = TEXT("Skill.Convergence");
		Skill.ItemInstanceId = FGuid::NewGuid();
		Skill.bCompatible = true;
		Panel->StageRune(Skill);
		const FKnightSkillBuild BeforeSubmit = Panel->GetDraftBuild();
		const int32 PreviousCount = Submitted.Num();
		Panel->SubmitCommand(Action);
		if (!TestEqual(TEXT("Each valid preset/build intent emits exactly one request"), Submitted.Num(), PreviousCount + 1))
		{
			break;
		}
		const FKnightRPGPanelCommand& Intent = Submitted.Last();
		TestTrue(TEXT("Constructing a command preserves the user's staged build"), Panel->GetDraftBuild() == BeforeSubmit);
		if (Action == EKnightRPGPanelAction::ApplyBuild)
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
			if (Action == EKnightRPGPanelAction::NewTalentLoadout)
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
					Intent.ProposedBuild == FKnightSkillBuild() && Intent.TalentLoadoutId == LoadoutId);
			}
			const bool bNamed = Action == EKnightRPGPanelAction::NewTalentLoadout
				|| Action == EKnightRPGPanelAction::RenameTalentLoadout
				|| Action == EKnightRPGPanelAction::DuplicateTalentLoadout;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightGearPanelSelectionTest,
	"Knight.M56.UI.InspectionAndTransferSelections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightGearPanelSelectionTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UKnightRPGPanel> Panel(NewObject<UKnightRPGPanel>());
	FKnightRPGPanelSnapshot Confirmed;
	Confirmed.KnightId = FKnightEntityId::Create();
	Confirmed.Gear.KnightId = Confirmed.KnightId;
	Confirmed.Gear.bEnabled = true;
	Confirmed.Gear.bCanEdit = true;
	Confirmed.Gear.InventoryRevision = 10;
	const TCHAR* Names[] = {TEXT("Sky"), TEXT("Sea"), TEXT("Earth")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		FKnightGearKnightRow Knight;
		Knight.KnightId = Index == 0 ? Confirmed.KnightId : FKnightEntityId::Create();
		Knight.Name = FText::FromString(Names[Index]);
		Knight.Revision = 20 + Index;
		Confirmed.Gear.Knights.Add(Knight);
	}
	FKnightGearItemRow Weapon;
	Weapon.InstanceId = FGuid::NewGuid();
	Weapon.AssignedKnightId = Confirmed.KnightId;
	Weapon.Category = TEXT("Equipment");
	Weapon.Subtype = TEXT("Hammer");
	Weapon.LocationId = TEXT("Equipment");
	Weapon.ItemRevision = 4;
	Confirmed.Gear.Items.Add(Weapon);
	Panel->SetSnapshot(Confirmed);
	TArray<FKnightGearPanelIntent> Submitted;
	const FDelegateHandle Handle = Panel->OnGearCommandRequested.AddLambda(
		[&Submitted](const FKnightGearPanelIntent& Intent)
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
	Inspection->OnSelectionChanged.AddDynamic(Panel.Get(), &UKnightRPGPanel::HandleGearKnightSelected);
	Panel->GearTransferSelector->OnSelectionChanged.AddDynamic(Panel.Get(), &UKnightRPGPanel::HandleGearTransferSelected);
	Panel->GearTransferSelector->SetSelectedOption(TEXT("Sea"));
	TestEqual(TEXT("Selecting transfer participant sends no domain or inspection command"), Submitted.Num(), 0);
	TestTrue(TEXT("Transfer destination is the exact selected canonical Knight"),
		Panel->GearTransferTarget == Confirmed.Gear.Knights[1].KnightId);
	Inspection->SetSelectedOption(TEXT("Sea"));
	TestEqual(TEXT("Inspecting the same name as transfer target still publishes a view selection"), Submitted.Num(), 1);
	if (Submitted.Num() == 1)
	{
		TestTrue(TEXT("Inspection has the correct view-only action and exact identity"),
			Submitted[0].Action == EKnightGearPanelAction::SelectKnight &&
			Submitted[0].KnightId == Confirmed.Gear.Knights[1].KnightId && !Submitted[0].RequestId.IsValid());
	}
	Panel->SelectGearItem(Weapon.InstanceId, false);
	Panel->StageGearAction(EKnightGearPanelAction::Transfer);
	TestTrue(TEXT("Transfer draft retains exact source and destination revisions"), Panel->HasGearDraft() &&
		Panel->GetGearDraft().KnightId == Confirmed.KnightId &&
		Panel->GetGearDraft().TargetKnightId == Confirmed.Gear.Knights[1].KnightId &&
		Panel->GetGearDraft().ExpectedKnightRevision == 20 &&
		Panel->GetGearDraft().ExpectedTargetKnightRevision == 21);
	Panel->GearTransferSelector->SetSelectedOption(TEXT("Earth"));
	TestFalse(TEXT("Changing the destination invalidates the reviewed transfer draft"), Panel->HasGearDraft());
	TestTrue(TEXT("Changed destination is exact and never changes confirmed ownership"),
		Panel->GearTransferTarget == Confirmed.Gear.Knights[2].KnightId &&
		Panel->GetSnapshot().Gear.Items[0].AssignedKnightId == Confirmed.KnightId);
	TestEqual(TEXT("Target changes do not submit or automatically retry authority mutations"), Submitted.Num(), 1);
	Panel->OnGearCommandRequested.Remove(Handle);
	return true;
}

#endif
