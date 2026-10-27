#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Definitions/KnightCampaignDefinition.h"
#include "Definitions/KnightConversationDefinition.h"
#include "Definitions/KnightStoryWorldDefinition.h"
#include "Engine/World.h"
#include "Events/KnightEventSubsystem.h"
#include "Runtime/KnightDialogueComponent.h"
#include "Runtime/KnightQuestComponent.h"
#include "Runtime/KnightStoryWorldComponent.h"

namespace KnightStoryTests
{
	class FWorld final
	{
	public:
		FWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Values);
			Owner = World->SpawnActor<AActor>();
			Quest = NewObject<UKnightQuestComponent>(Owner);
			Owner->AddInstanceComponent(Quest);
			Quest->RegisterComponent();
			WorldState = NewObject<UKnightStoryWorldComponent>(Owner);
			Owner->AddInstanceComponent(WorldState);
			WorldState->RegisterComponent();
		}
		~FWorld()
		{
			World->DestroyWorld(false);
		}
		UWorld* World = nullptr;
		AActor* Owner = nullptr;
		UKnightQuestComponent* Quest = nullptr;
		UKnightStoryWorldComponent* WorldState = nullptr;
	};

	UKnightCampaignDefinition* Definition()
	{
		UKnightCampaignDefinition* Result = NewObject<UKnightCampaignDefinition>();
		Result->TrySetIdentity(TEXT("DomainCampaign"), TEXT("KnightCampaign"));
		Result->CampaignId = TEXT("DomainCampaign");
		Result->ScenarioKind = TEXT("Test");
		Result->ActIds = {TEXT("Act")};
		FKnightQuestDefinition& Quest = Result->Quests.AddDefaulted_GetRef();
		Quest.QuestId = TEXT("Ordered");
		Quest.ActId = TEXT("Act");
		Quest.bOrdered = true;
		for (const FName Stage : {FName(TEXT("First")), FName(TEXT("Second"))})
		{
			FKnightObjectiveDefinition& Objective = Quest.Objectives.AddDefaulted_GetRef();
			Objective.ObjectiveId = Stage;
			Objective.EventName = TEXT("Interacted");
			Objective.SubjectId = Stage;
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightStoryDefinitionTest, "Knight.M6.DefinitionValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightStoryDefinitionTest::RunTest(const FString& Parameters)
{
	UKnightCampaignDefinition* Campaign = KnightStoryTests::Definition();
	TestTrue(TEXT("Supported quest definition"), Campaign->ValidateCampaign().IsSuccess());
	const FKnightQuestDefinition Duplicate = Campaign->Quests[0];
	Campaign->Quests.Add(Duplicate);
	TestFalse(TEXT("Duplicate quest IDs"), Campaign->ValidateCampaign().IsSuccess());
	Campaign->Quests.RemoveAt(1);
	Campaign->Quests[0].Prerequisites.Add(TEXT("Missing"));
	TestFalse(TEXT("Missing prerequisite"), Campaign->ValidateCampaign().IsSuccess());
	Campaign->Quests[0].Prerequisites = {TEXT("Ordered")};
	TestFalse(TEXT("Self cycle"), Campaign->ValidateCampaign().IsSuccess());
	Campaign->Quests[0].Prerequisites.Reset();
	Campaign->Quests[0].Objectives[1].ObjectiveId = TEXT("First");
	TestFalse(TEXT("Duplicate objective IDs"), Campaign->ValidateCampaign().IsSuccess());
	UKnightStoryWorldDefinition* World = NewObject<UKnightStoryWorldDefinition>();
	World->TrySetIdentity(TEXT("ValidatorWorld"), TEXT("KnightStoryWorld"));
	FKnightStoryInteraction& Interaction = World->Interactions.AddDefaulted_GetRef();
	Interaction.AnchorId = TEXT("Station");
	Interaction.Label = FText::FromString(TEXT("Station"));
	Interaction.SubjectId = TEXT("Station");
	TestTrue(TEXT("Valid world interaction"), World->ValidateWorld().IsSuccess());
	Interaction.RequiredObjectiveId = TEXT("Orphan");
	TestFalse(TEXT("Objective gate requires a quest reference"), World->ValidateWorld().IsSuccess());
	Interaction.RequiredObjectiveId = NAME_None;
	Interaction.Action = static_cast<EKnightStoryAction>(255);
	TestFalse(TEXT("Unknown action rejected before dispatch"), World->ValidateWorld().IsSuccess());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightStoryFactTest, "Knight.M6.ConfirmedFactOrderAndRuns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightStoryFactTest::RunTest(const FString& Parameters)
{
	KnightStoryTests::FWorld Fixture;
	UKnightCampaignDefinition* Campaign = KnightStoryTests::Definition();
	const FGuid RunId = FGuid::NewGuid();
	TestTrue(TEXT("Quest initialized"), Fixture.Quest->InitializeCampaign(Campaign, RunId).IsSuccess());
	TestTrue(TEXT("World initialized"), Fixture.WorldState->InitializeWorld(Campaign->CampaignId, RunId).IsSuccess());
	Fixture.WorldState->CommitFact(TEXT("Interacted"), TEXT("Second"));
	TestEqual(TEXT("Out of order fact not credited"), Fixture.Quest->FindProgress(TEXT("Ordered"))->Counts.FindRef(TEXT("Second")), 0);
	const FGuid Confirmation = FGuid::NewGuid();
	Fixture.WorldState->CommitFact(TEXT("Interacted"), TEXT("First"), NAME_None, true, Confirmation);
	Fixture.WorldState->CommitFact(TEXT("Interacted"), TEXT("First"), NAME_None, true, Confirmation);
	TestEqual(TEXT("Duplicate confirmation credited once"), Fixture.Quest->FindProgress(TEXT("Ordered"))->Counts.FindRef(TEXT("First")), 1);
	FKnightStoryEvent Wrong;
	Wrong.ConfirmationId = FGuid::NewGuid();
	Wrong.SourceId = FKnightEntityId::Create();
	Wrong.Revision = 99;
	Wrong.RunId = FGuid::NewGuid();
	Wrong.CampaignId = Campaign->CampaignId;
	Wrong.EventName = TEXT("Interacted");
	Wrong.SubjectId = TEXT("Second");
	Fixture.World->GetSubsystem<UKnightEventSubsystem>()->PublishConfirmed(Wrong);
	TestEqual(TEXT("Wrong run ignored"), Fixture.Quest->FindProgress(TEXT("Ordered"))->Counts.FindRef(TEXT("Second")), 0);
	Fixture.WorldState->CommitFact(TEXT("Interacted"), TEXT("Second"));
	TestEqual(TEXT("Confirmed sequence completes"), Fixture.Quest->FindProgress(TEXT("Ordered"))->Phase, EKnightQuestPhase::Complete);
	const FKnightQuestSnapshot Before = Fixture.Quest->GetSnapshot();
	FKnightQuestSnapshot Invalid = Before;
	Invalid.Quests[0].Counts.FindChecked(TEXT("Second")) = 2;
	TestFalse(TEXT("Bad count cannot load"), Fixture.Quest->RestoreSnapshot(Invalid).IsSuccess());
	TestEqual(TEXT("Rejected restore preserves state"), Fixture.Quest->GetSnapshot().Revision, Before.Revision);
	Fixture.WorldState->BeginEncounter();
	const FGuid OldEncounter = Fixture.WorldState->GetSnapshot().EncounterRunId;
	Fixture.WorldState->CommitFact(TEXT("ArmorState"), TEXT("Armor"), TEXT("Broken"), false);
	Fixture.WorldState->FinishEncounter(false, OldEncounter);
	Fixture.WorldState->BeginEncounter();
	TestFalse(TEXT("Wrong encounter outcome rejected"), Fixture.WorldState->FinishEncounter(true, OldEncounter).IsSuccess());
	TestFalse(TEXT("Transient armor fact resets"), Fixture.WorldState->GetSnapshot().Facts.ContainsByPredicate([](const FKnightWorldFact& Fact)
	{
		return Fact.EventName == TEXT("ArmorState");
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightStoryActivationTest, "Knight.M6.ActivationAndAdditiveRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightStoryActivationTest::RunTest(const FString& Parameters)
{
	KnightStoryTests::FWorld Fixture;
	UKnightCampaignDefinition* Campaign = KnightStoryTests::Definition();
	const FGuid RunId = FGuid::NewGuid();
	Fixture.Quest->InitializeCampaign(Campaign, RunId);
	Fixture.WorldState->InitializeWorld(Campaign->CampaignId, RunId);
	Fixture.WorldState->CommitFact(TEXT("Interacted"), TEXT("First"));
	Fixture.WorldState->CommitFact(TEXT("Interacted"), TEXT("Second"));
	const FKnightQuestSnapshot V1 = Fixture.Quest->GetSnapshot();
	Campaign->ContentRevision = 2;
	FKnightQuestDefinition& Added = Campaign->Quests.AddDefaulted_GetRef();
	Added.QuestId = TEXT("Appended");
	Added.IntroducedRevision = 2;
	Added.ActId = TEXT("Act");
	Added.Prerequisites = {TEXT("Ordered")};
	Added.bAutoActivate = false;
	FKnightObjectiveDefinition& Objective = Added.Objectives.AddDefaulted_GetRef();
	Objective.ObjectiveId = TEXT("CollectState");
	Objective.SubjectId = TEXT("Ore");
	Objective.EventName = TEXT("InventoryState");
	Objective.RequiredCount = 3;
	Objective.Policy = EKnightObjectivePolicy::CurrentState;
	TestTrue(TEXT("Append without runtime rewrite"), Fixture.Quest->RestoreSnapshot(V1).IsSuccess());
	TestEqual(TEXT("Old completion retained"), Fixture.Quest->FindProgress(TEXT("Ordered"))->Phase, EKnightQuestPhase::Complete);
	TestEqual(TEXT("New quest available"), Fixture.Quest->FindProgress(TEXT("Appended"))->Phase, EKnightQuestPhase::Available);
	TestEqual(TEXT("Immutable V1 content revision"), V1.ContentRevision, 1);
	Fixture.Quest->ActivateQuest(TEXT("Appended"));
	FKnightStoryEvent Current;
	Current.CampaignId = Campaign->CampaignId;
	Current.RunId = RunId;
	Current.EventName = TEXT("InventoryState");
	Current.SubjectId = TEXT("Ore");
	Current.Quantity = 3;
	Fixture.Quest->ApplyCurrentFact(Current);
	TestEqual(TEXT("Explicit current-state predicate"), Fixture.Quest->FindProgress(TEXT("Appended"))->Phase, EKnightQuestPhase::Complete);
	TestTrue(TEXT("Branch commit"), Fixture.Quest->CommitBranch(TEXT("Gate"), TEXT("Left"), Fixture.Quest->GetSnapshot().Revision).IsSuccess());
	TestFalse(TEXT("Branch cannot be rewritten"), Fixture.Quest->CommitBranch(TEXT("Gate"), TEXT("Right"), Fixture.Quest->GetSnapshot().Revision).IsSuccess());
	TestEqual(TEXT("Branch retained"), *Fixture.Quest->GetSnapshot().Branches.Find(TEXT("Gate")), FName(TEXT("Left")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightStoryDialogueTest, "Knight.M6.DialogueHostCancelAndDurableChoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnightStoryDialogueTest::RunTest(const FString& Parameters)
{
	KnightStoryTests::FWorld Fixture;
	UKnightDialogueComponent* Dialogue = NewObject<UKnightDialogueComponent>(Fixture.Owner);
	Fixture.Owner->AddInstanceComponent(Dialogue);
	Dialogue->RegisterComponent();
	UKnightConversationDefinition* Definition = NewObject<UKnightConversationDefinition>();
	Definition->TrySetIdentity(TEXT("ChoiceConversation"), TEXT("KnightConversation"));
	Definition->NpcId = TEXT("SyntheticNpc");
	Definition->FirstNodeId = TEXT("Choice");
	FKnightDialogueNode Choice;
	Choice.NodeId = TEXT("Choice");
	Choice.Text = FText::FromString(TEXT("Choose a route"));
	Choice.Choices.Add(TEXT("Left"), TEXT("End"));
	Choice.Choices.Add(TEXT("Right"), TEXT("End"));
	FKnightDialogueNode End;
	End.NodeId = TEXT("End");
	End.Text = FText::FromString(TEXT("Confirmed route"));
	Definition->Nodes = {Choice, End};
	TestTrue(TEXT("Choice graph validates"), Definition->ValidateConversation().IsSuccess());
	TestTrue(TEXT("Real conversation starts"), Dialogue->StartConversation(Definition).IsSuccess());
	bool bFinished = false;
	const int64 InitialRevision = Dialogue->GetSnapshot().Revision;
	TestFalse(TEXT("Guest cannot advance shared cursor"),
		Dialogue->AdvanceConversation(false, InitialRevision, TEXT("Left"), bFinished).IsSuccess());
	TestEqual(TEXT("Rejected guest intent retains cursor revision"), Dialogue->GetSnapshot().Revision, InitialRevision);
	TestFalse(TEXT("Unauthored choice rejected"),
		Dialogue->AdvanceConversation(true, InitialRevision, TEXT("Unknown"), bFinished).IsSuccess());
	Dialogue->CancelConversation();
	TestFalse(TEXT("Cancel has no completion"), bFinished);
	TestTrue(TEXT("Cancel allows re-entry"), Dialogue->StartConversation(Definition).IsSuccess());
	TestEqual(TEXT("Re-entry uses authored first node"), Dialogue->GetSnapshot().NodeId, Definition->FirstNodeId);
	TestTrue(TEXT("Host choice commits"),
		Dialogue->AdvanceConversation(true, Dialogue->GetSnapshot().Revision, TEXT("Left"), bFinished).IsSuccess());
	TestFalse(TEXT("Choice step is not final completion"), bFinished);
	TestTrue(TEXT("Final node completes"),
		Dialogue->AdvanceConversation(true, Dialogue->GetSnapshot().Revision, NAME_None, bFinished).IsSuccess() && bFinished);
	const FKnightDialogueSnapshot Saved = Dialogue->GetSnapshot();
	TestTrue(TEXT("Completed choice is retained independently of actor"),
		Saved.Choices.Find(TEXT("ChoiceConversation_Choice")) != nullptr
		&& *Saved.Choices.Find(TEXT("ChoiceConversation_Choice")) == TEXT("Left"));
	UKnightDialogueComponent* Restored = NewObject<UKnightDialogueComponent>(Fixture.Owner);
	Fixture.Owner->AddInstanceComponent(Restored);
	Restored->RegisterComponent();
	TestTrue(TEXT("New participant restores durable choice"), Restored->RestoreChoices(Saved).IsSuccess());
	TestTrue(TEXT("Restored conversation re-entry"), Restored->StartConversation(Definition).IsSuccess());
	TestFalse(TEXT("Re-entry cannot rewrite retained choice"),
		Restored->AdvanceConversation(true, Restored->GetSnapshot().Revision, TEXT("Right"), bFinished).IsSuccess());
	const FKnightDialogueNode Duplicate = Definition->Nodes[0];
	Definition->Nodes.Add(Duplicate);
	TestFalse(TEXT("Duplicate dialogue nodes rejected"), Definition->ValidateConversation().IsSuccess());
	return true;
}

#endif
