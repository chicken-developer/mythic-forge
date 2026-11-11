#include "Commandlets/GolemStoryAssetAuthoring.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Definitions/GolemCampaignDefinition.h"
#include "Definitions/GolemConversationDefinition.h"
#include "Definitions/GolemItemDefinition.h"
#include "Definitions/GolemStoryWorldDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interaction/GolemStoryAnchor.h"
#include "PartyArena/GolemPartyArenaStartMarker.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Story/GolemStoryEntry.h"
#include "Story/GolemProtectedCaravan.h"
#include "Story/GolemStoryProfile.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
	template <typename TAsset>
	TAsset* Asset(const FString& Path, const FName Type, const FName Id)
	{
		const bool bExists = FPackageName::DoesPackageExist(Path);
		UPackage* Package = bExists ? LoadPackage(nullptr, *Path, LOAD_None) : CreatePackage(*Path);
		if (Package == nullptr)
		{
			return nullptr;
		}
		const FName Name(*FPackageName::GetLongPackageAssetName(Path));
		TAsset* Result = FindObject<TAsset>(Package, *Name.ToString());
		if (Result == nullptr && !bExists)
		{
			Result = NewObject<TAsset>(Package, Name, RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Result);
		}
		if (Result == nullptr || !Result->TrySetIdentity(Id, Type))
		{
			return nullptr;
		}
		return Result;
	}

	bool Save(UGolemDefinition* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		return UPackage::SavePackage(Package, Asset,
			*Filename, Args);
	}

	FGolemObjectiveDefinition Objective(const FName Id, const FText& Label, const FName Event, const FName Subject)
	{
		FGolemObjectiveDefinition Result;
		Result.ObjectiveId = Id;
		Result.Label = Label;
		Result.EventName = Event;
		Result.SubjectId = Subject;
		return Result;
	}

	FGolemStoryInteraction Interaction(const FName AnchorId, const FText& Label, const EGolemStoryAction Action,
		const FName Event, const FName Subject)
	{
		FGolemStoryInteraction Result;
		Result.AnchorId = AnchorId;
		Result.Label = Label;
		Result.Action = Action;
		Result.EventName = Event;
		Result.SubjectId = Subject;
		return Result;
	}

	const FString Root(TEXT("/Game/Knight/Definitions/Story/"));
}

bool GolemStoryAssetAuthoring::CreateStoryAssets()
{
	UGolemConversationDefinition* Boro = Asset<UGolemConversationDefinition>(
		Root + TEXT("DA_Knight_Conversation_Boro"), TEXT("KnightConversation"), TEXT("BoroLesson"));
	if (Boro == nullptr)
	{
		return false;
	}
	Boro->NpcId = TEXT("Boro");
	Boro->FirstNodeId = TEXT("Introduction");
	Boro->Nodes.Reset();
	FGolemDialogueNode& Intro = Boro->Nodes.AddDefaulted_GetRef();
	Intro.NodeId = TEXT("Introduction");
	Intro.NextNodeId = TEXT("Models");
	Intro.Text = NSLOCTEXT("KnightStory", "BoroIntro",
		"Boro: Three ancient hammers vanished after the ancient battle. Each carried a different group of elements.");
	FGolemDialogueNode& LessonModels = Boro->Nodes.AddDefaulted_GetRef();
	LessonModels.NodeId = TEXT("Models");
	LessonModels.Text = NSLOCTEXT("KnightStory", "BoroModels",
		"Boro: Examine all three models. Then choose a head and a handle, heat, hammer, cool and finish your first simple hammer.");
	if (!Boro->ValidateConversation().IsSuccess() || !Save(Boro))
	{
		return false;
	}
	for (int32 Revision = 1; Revision <= 2; ++Revision)
	{
		UGolemCampaignDefinition* Campaign = Asset<UGolemCampaignDefinition>(
			Root + FString::Printf(TEXT("DA_Knight_Campaign_DemoV%d"), Revision),
			TEXT("KnightCampaign"), FName(*FString::Printf(TEXT("Demo_M6_V%d"), Revision)));
		if (Campaign == nullptr)
		{
			return false;
		}
		Campaign->CampaignId = TEXT("Demo_M6");
		Campaign->ScenarioKind = TEXT("StoryDemo");
		Campaign->ContentRevision = Revision;
		Campaign->ActIds = {TEXT("Act_I")};
		Campaign->Quests.Reset();
		FGolemQuestDefinition& Lesson = Campaign->Quests.AddDefaulted_GetRef();
		Lesson.QuestId = TEXT("ActI_Q1");
		Lesson.ActId = TEXT("Act_I");
		Lesson.Title = NSLOCTEXT("KnightStory", "Q1Title", "Q1 — The lesson of three ancient hammers");
		Lesson.Objectives = {
			Objective(TEXT("Q1_Boro"), NSLOCTEXT("KnightStory", "Q1Talk", "Listen to Boro [F]"), TEXT("ConversationFinished"), TEXT("Boro")),
			Objective(TEXT("Q1_Sky"), NSLOCTEXT("KnightStory", "Q1Sky", "Examine the Sky model [F]"), TEXT("Interacted"), TEXT("Model_Sky")),
			Objective(TEXT("Q1_Sea"), NSLOCTEXT("KnightStory", "Q1Sea", "Examine the Sea model [F]"), TEXT("Interacted"), TEXT("Model_Sea")),
			Objective(TEXT("Q1_Earth"), NSLOCTEXT("KnightStory", "Q1Earth", "Examine the Earth model [F]"), TEXT("Interacted"), TEXT("Model_Earth"))};
		FGolemQuestDefinition& Forge = Campaign->Quests.AddDefaulted_GetRef();
		Forge.QuestId = TEXT("ActI_Q2");
		Forge.ActId = TEXT("Act_I");
		Forge.Title = NSLOCTEXT("KnightStory", "Q2Title", "Q2 — Forge the first hammer");
		Forge.Prerequisites = {TEXT("ActI_Q1")};
		Forge.bOrdered = true;
		Forge.RewardKey = TEXT("FirstHammers");
		const FName Stages[]{TEXT("Head"), TEXT("Handle"), TEXT("Heat"), TEXT("Hammer"), TEXT("Cool"), TEXT("Finish")};
		const FText StageLabels[]{
			NSLOCTEXT("KnightStory", "Head", "Select a simple stone head"),
			NSLOCTEXT("KnightStory", "Handle", "Select a wooden handle"),
			NSLOCTEXT("KnightStory", "Heat", "Heat at the furnace"),
			NSLOCTEXT("KnightStory", "Hammer", "Hammer at the anvil"),
			NSLOCTEXT("KnightStory", "Cool", "Cool in water"),
			NSLOCTEXT("KnightStory", "Finish", "Finish the three simple hammers")};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Stages); ++Index)
		{
			const FName ObjectiveId(*(TEXT("Q2_") + Stages[Index].ToString()));
			Forge.Objectives.Add(Objective(ObjectiveId, StageLabels[Index], TEXT("ForgeCommitted"), Stages[Index]));
		}
		if (Revision == 2)
		{
			FGolemQuestDefinition& Append = Campaign->Quests.AddDefaulted_GetRef();
			Append.QuestId = TEXT("Demo_Append");
			Append.IntroducedRevision = 2;
			Append.ActId = TEXT("Act_I");
			Append.Title = NSLOCTEXT("KnightStory", "AppendTitle", "Append fixture — Inspect the notice board");
			Append.Prerequisites = {TEXT("ActI_Q2")};
			Append.bAutoActivate = false;
			Append.Objectives.Add(Objective(TEXT("Append_Notice"),
				NSLOCTEXT("KnightStory", "Notice", "Inspect the notice board"), TEXT("Interacted"), TEXT("Notice")));
		}
		if (!Campaign->ValidateCampaign().IsSuccess() || !Save(Campaign))
		{
			return false;
		}
		UGolemStoryProfile* Profile = Asset<UGolemStoryProfile>(
			Root + FString::Printf(TEXT("DA_Knight_Profile_DemoV%d"), Revision), TEXT("KnightStoryProfile"),
			FName(*FString::Printf(TEXT("Demo_M6_V%d"), Revision)));
		if (Profile == nullptr)
		{
			return false;
		}
		Profile->CampaignDefinitionId = Campaign->GetPrimaryAssetId();
		Profile->WorldDefinitionId = FPrimaryAssetId(TEXT("KnightStoryWorld"), TEXT("ForgeSchool"));
		Profile->bDeveloperFixture = false;
		Profile->Rewards.Reset();
		FGolemStoryReward& Reward = Profile->Rewards.AddDefaulted_GetRef();
		Reward.QuestId = TEXT("ActI_Q2");
		Reward.RewardKey = TEXT("FirstHammers");
		Reward.bReuseExistingHammers = true;
		for (const FName Hammer : {FName(TEXT("Tutorial_Sky")), FName(TEXT("Tutorial_Sea")), FName(TEXT("Tutorial_Earth"))})
		{
			FGolemGearRewardItem& Item = Reward.Items.AddDefaulted_GetRef();
			Item.DefinitionId = FPrimaryAssetId(TEXT("KnightItem"), Hammer);
			Item.Count = 1;
		}
		if (!Save(Profile))
		{
			return false;
		}
	}
	UGolemItemDefinition* Starter = LoadObject<UGolemItemDefinition>(nullptr,
		TEXT("/Game/Knight/Definitions/Items/DA_Knight_Item_M056_W01.DA_Knight_Item_M056_W01"));
	if (Starter == nullptr)
	{
		return false;
	}
	const FName HammerIds[]{TEXT("Tutorial_Sky"), TEXT("Tutorial_Sea"), TEXT("Tutorial_Earth")};
	const FName Themes[]{TEXT("W01"), TEXT("W03"), TEXT("W04")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(HammerIds); ++Index)
	{
		UGolemItemDefinition* Hammer = Asset<UGolemItemDefinition>(Root + TEXT("DA_Knight_Item_")
			+ HammerIds[Index].ToString(), TEXT("KnightItem"), HammerIds[Index]);
		if (Hammer == nullptr)
		{
			return false;
		}
		FGolemItemMetadata Metadata = Starter->GetMetadata();
		Metadata.Theme = Themes[Index];
		Hammer->SetDisplayName(FText::Format(NSLOCTEXT("KnightStory", "SimpleHammer", "Simple {0} hammer"),
			FText::FromName(HammerIds[Index])));
		const FGolemResult Result = Hammer->TrySetItemTuning(Starter->GetItemKind(), Starter->GetMaximumStack(),
			Starter->GetAbilitySetId(), Starter->GetStatBonuses());
		if (!Result.IsSuccess())
		{
			return false;
		}
		if (!Hammer->TrySetMetadata(Metadata).IsSuccess())
		{
			return false;
		}
		if (!Hammer->ValidateItem().IsSuccess() || !Save(Hammer))
		{
			return false;
		}
	}
	UGolemStoryWorldDefinition* World = Asset<UGolemStoryWorldDefinition>(
		Root + TEXT("DA_Knight_World_ForgeSchool"), TEXT("KnightStoryWorld"), TEXT("ForgeSchool"));
	if (World == nullptr)
	{
		return false;
	}
	World->Interactions.Reset();
	FGolemStoryInteraction BoroInteraction = Interaction(TEXT("School_Boro"),
		NSLOCTEXT("KnightStory", "BoroMarker", "BORO [F]"), EGolemStoryAction::Conversation, NAME_None, TEXT("Boro"));
	BoroInteraction.ConversationId = Boro->GetPrimaryAssetId();
	World->Interactions.Add(BoroInteraction);
	const FName Models[]{TEXT("Model_Sky"), TEXT("Model_Sea"), TEXT("Model_Earth")};
	const FText ModelLabels[]{NSLOCTEXT("KnightStory", "SkyMarker", "SKY MODEL [F]"),
		NSLOCTEXT("KnightStory", "SeaMarker", "SEA MODEL [F]"), NSLOCTEXT("KnightStory", "EarthMarker", "EARTH MODEL [F]")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Models); ++Index)
	{
		World->Interactions.Add(Interaction(Models[Index], ModelLabels[Index],
			EGolemStoryAction::Fact, TEXT("Interacted"), Models[Index]));
	}
	const FName Stages[]{TEXT("Head"), TEXT("Handle"), TEXT("Heat"), TEXT("Hammer"), TEXT("Cool"), TEXT("Finish")};
	for (const FName Stage : Stages)
	{
		FGolemStoryInteraction Row = Interaction(FName(*(TEXT("Forge_") + Stage.ToString())),
			FText::Format(NSLOCTEXT("KnightStory", "StageMarker", "{0} [F]"), FText::FromName(Stage)),
			EGolemStoryAction::Forge, TEXT("ForgeCommitted"), Stage);
		Row.RequiredQuestId = TEXT("ActI_Q2");
		Row.RequiredObjectiveId = FName(*(TEXT("Q2_") + Stage.ToString()));
		Row.ValueId = TEXT("Simple");
		if (Stage == TEXT("Head"))
		{
			Row.AllowedChoices = {TEXT("Stone"), TEXT("Bronze")};
			Row.ValueId = TEXT("Stone");
		}
		else if (Stage == TEXT("Handle"))
		{
			Row.AllowedChoices = {TEXT("Wood"), TEXT("Oak")};
			Row.ValueId = TEXT("Wood");
		}
		World->Interactions.Add(Row);
	}
	FGolemStoryInteraction Claim = Interaction(TEXT("School_Claim"),
		NSLOCTEXT("KnightStory", "ClaimMarker", "CLAIM HAMMERS [F]"), EGolemStoryAction::Reward, NAME_None, NAME_None);
	Claim.RewardQuestId = TEXT("ActI_Q2");
	World->Interactions.Add(Claim);
	World->Interactions.Add(Interaction(TEXT("School_Checkpoint"), NSLOCTEXT("KnightStory", "SaveMarker", "CHECKPOINT [F]"),
		EGolemStoryAction::Checkpoint, NAME_None, NAME_None));
	FGolemStoryInteraction Travel = Interaction(TEXT("School_Travel"),
		NSLOCTEXT("KnightStory", "TravelMarker", "COURTYARD [F]"), EGolemStoryAction::Travel, NAME_None, NAME_None);
	Travel.Destination = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Knight/Maps/Playables/L_M06_StoryCourtyard.L_M06_StoryCourtyard")));
	World->Interactions.Add(Travel);
	World->Interactions.Add(Interaction(TEXT("School_Notice"), NSLOCTEXT("KnightStory", "NoticeMarker", "NOTICE [F]"),
		EGolemStoryAction::Fact, TEXT("Interacted"), TEXT("Notice")));
	if (!World->ValidateWorld().IsSuccess() || !Save(World))
	{
		return false;
	}
	for (const bool bCaravan : {true, false})
	{
		const FName Suffix = bCaravan ? FName(TEXT("Caravan")) : FName(TEXT("Lifecycle"));
		UGolemCampaignDefinition* FixtureCampaign = Asset<UGolemCampaignDefinition>(
			Root + TEXT("DA_Knight_Campaign_") + Suffix.ToString(), TEXT("KnightCampaign"), Suffix);
		UGolemStoryProfile* FixtureProfile = Asset<UGolemStoryProfile>(
			Root + TEXT("DA_Knight_Profile_") + Suffix.ToString(), TEXT("KnightStoryProfile"), Suffix);
		UGolemStoryWorldDefinition* FixtureWorld = Asset<UGolemStoryWorldDefinition>(
			Root + TEXT("DA_Knight_World_") + Suffix.ToString(), TEXT("KnightStoryWorld"), Suffix);
		if (FixtureCampaign == nullptr || FixtureProfile == nullptr || FixtureWorld == nullptr)
		{
			return false;
		}
		FixtureCampaign->CampaignId = FName(*(TEXT("Fixture_M6_") + Suffix.ToString()));
		FixtureCampaign->ScenarioKind = TEXT("DeveloperFixture");
		FixtureCampaign->ContentRevision = bCaravan ? 1 : 2;
		FixtureCampaign->ActIds = {TEXT("Fixture")};
		FixtureCampaign->Quests.Reset();
		FGolemQuestDefinition& Quest = FixtureCampaign->Quests.AddDefaulted_GetRef();
		Quest.QuestId = bCaravan ? TEXT("Fixture_Q5") : TEXT("Fixture_Scene");
		Quest.ActId = TEXT("Fixture");
		Quest.Title = bCaravan ? NSLOCTEXT("KnightStory", "Q5Fixture", "Q5 fixture — Protect the caravan")
			: NSLOCTEXT("KnightStory", "SceneFixture", "Scene lifecycle fixture");
		Quest.RewardKey = bCaravan ? FName(TEXT("CaravanOnce")) : NAME_None;
		FGolemObjectiveDefinition Goal = Objective(bCaravan ? TEXT("Q5_Protect") : TEXT("Scene_Outcome"),
			bCaravan ? NSLOCTEXT("KnightStory", "Protect", "Protect the caravan and defeat the wave")
				: NSLOCTEXT("KnightStory", "SceneOutcome", "Start, then skip or cancel the scene"),
			bCaravan ? TEXT("EncounterCompleted") : TEXT("SceneFinished"), bCaravan ? TEXT("Caravan") : TEXT("Synthetic"));
		Goal.bEncounterScoped = bCaravan;
		Quest.Objectives.Add(Goal);
		FixtureProfile->CampaignDefinitionId = FixtureCampaign->GetPrimaryAssetId();
		FixtureProfile->WorldDefinitionId = FixtureWorld->GetPrimaryAssetId();
		FixtureProfile->bDeveloperFixture = true;
		FixtureProfile->DeveloperSeedItems.Reset();
		FGolemGearRewardItem& StarterSeed = FixtureProfile->DeveloperSeedItems.AddDefaulted_GetRef();
		StarterSeed.DefinitionId = FPrimaryAssetId(TEXT("KnightItem"), TEXT("M056_W01"));
		StarterSeed.Count = 3;
		if (!bCaravan)
		{
			FGolemGearRewardItem& MaterialSeed = FixtureProfile->DeveloperSeedItems.AddDefaulted_GetRef();
			MaterialSeed.DefinitionId = FPrimaryAssetId(TEXT("KnightItem"), TEXT("M056_M01"));
			MaterialSeed.Count = 1;
			FGolemQuestDefinition& Collect = FixtureCampaign->Quests.AddDefaulted_GetRef();
			Collect.QuestId = TEXT("Fixture_Collect");
			Collect.ActId = TEXT("Fixture");
			Collect.Title = NSLOCTEXT("KnightStory", "CollectFixture", "Collect/deliver fixture");
			Collect.bOrdered = true;
			Collect.bAutoActivate = false;
			FGolemObjectiveDefinition Current = Objective(TEXT("Fixture_CurrentItem"),
				NSLOCTEXT("KnightStory", "FixtureCurrent", "Confirm the current ore stack"), TEXT("InventoryState"), TEXT("KnightItem:M056_M01"));
			Current.Policy = EGolemObjectivePolicy::CurrentState;
			Collect.Objectives.Add(Current);
			Collect.Objectives.Add(Objective(TEXT("Fixture_DeliverItem"),
				NSLOCTEXT("KnightStory", "FixtureDeliver", "Deliver the ore"), TEXT("ItemDelivered"), TEXT("KnightItem:M056_M01")));
			FGolemQuestDefinition& Route = FixtureCampaign->Quests.AddDefaulted_GetRef();
			Route.QuestId = TEXT("Fixture_RouteQuest");
			Route.ActId = TEXT("Fixture");
			Route.IntroducedRevision = 2;
			Route.Prerequisites = {TEXT("Fixture_Scene")};
			Route.Title = NSLOCTEXT("KnightStory", "RouteFixture", "Synthetic route fixture");
			Route.Objectives.Add(Objective(TEXT("Fixture_RouteChoice"),
				NSLOCTEXT("KnightStory", "RouteChoiceFixture", "Choose Left or Right at the route marker"),
				TEXT("BranchCommitted"), TEXT("Fixture_Route")));
		}
		FixtureProfile->Rewards.Reset();
		if (bCaravan)
		{
			FGolemStoryReward& Reward = FixtureProfile->Rewards.AddDefaulted_GetRef();
			Reward.QuestId = Quest.QuestId;
			Reward.RewardKey = Quest.RewardKey;
			FGolemGearRewardItem& Item = Reward.Items.AddDefaulted_GetRef();
			Item.DefinitionId = FPrimaryAssetId(TEXT("KnightItem"), TEXT("M056_M01"));
			Item.Count = 1;
		}
		FixtureWorld->Interactions.Reset();
		FixtureWorld->Interactions.Add(Interaction(TEXT("Fixture_Start"),
			bCaravan ? NSLOCTEXT("KnightStory", "Q5Start", "START CARAVAN [F]")
				: NSLOCTEXT("KnightStory", "SceneStart", "START SCENE [F]"),
			bCaravan ? EGolemStoryAction::Encounter : EGolemStoryAction::Scene, NAME_None,
			bCaravan ? TEXT("Caravan") : TEXT("Synthetic")));
		if (!bCaravan)
		{
			FGolemStoryInteraction Deliver = Interaction(TEXT("Fixture_Deliver"),
				NSLOCTEXT("KnightStory", "DeliverMarker", "DELIVER ORE [F]"), EGolemStoryAction::Deliver,
				TEXT("ItemDelivered"), TEXT("KnightItem:M056_M01"));
			Deliver.RequiredQuestId = TEXT("Fixture_Collect");
			Deliver.RequiredObjectiveId = TEXT("Fixture_DeliverItem");
			Deliver.DeliveryItemId = FPrimaryAssetId(TEXT("KnightItem"), TEXT("M056_M01"));
			FixtureWorld->Interactions.Add(Deliver);
			FGolemStoryInteraction Branch = Interaction(TEXT("Fixture_Branch"),
				NSLOCTEXT("KnightStory", "BranchMarker", "CHOOSE ROUTE [F]"), EGolemStoryAction::Branch,
				TEXT("BranchCommitted"), TEXT("Fixture_Route"));
			Branch.AllowedChoices = {TEXT("Left"), TEXT("Right")};
			Branch.ValueId = TEXT("Left");
			Branch.RequiredQuestId = TEXT("Fixture_RouteQuest");
			Branch.RequiredObjectiveId = TEXT("Fixture_RouteChoice");
			FixtureWorld->Interactions.Add(Branch);
			FGolemStoryInteraction Gate = Interaction(TEXT("Fixture_Gate"),
				NSLOCTEXT("KnightStory", "GateMarker", "LEFT ROUTE GATE [F]"), EGolemStoryAction::Fact,
				TEXT("Interacted"), TEXT("Fixture_LeftGate"));
			Gate.RequiredBranchId = TEXT("Fixture_Route");
			Gate.RequiredBranchChoice = TEXT("Left");
			FixtureWorld->Interactions.Add(Gate);
		}
		if (bCaravan)
		{
			FGolemStoryInteraction FixtureClaim = Interaction(TEXT("Fixture_Claim"),
				NSLOCTEXT("KnightStory", "Q5Claim", "CLAIM ONCE [F]"), EGolemStoryAction::Reward, NAME_None, NAME_None);
			FixtureClaim.RewardQuestId = Quest.QuestId;
			FixtureWorld->Interactions.Add(FixtureClaim);
		}
		if (!FixtureCampaign->ValidateCampaign().IsSuccess() || !FixtureWorld->ValidateWorld().IsSuccess()
			|| !Save(FixtureCampaign) || !Save(FixtureProfile) || !Save(FixtureWorld))
		{
			return false;
		}
	}
	return true;
}

bool GolemStoryAssetAuthoring::PopulateStoryMap(UWorld* World, const int32 Milestone)
{
	if (Milestone == 6 || Milestone == 60 || Milestone == 63)
	{
		for (TActorIterator<AGolemPartyArenaStartMarker> It(World); It; ++It)
		{
			// The school uses its own authored lesson markers and input help.
			It->SetActorHiddenInGame(true);
		}
	}
	AGolemStoryEntry* Entry = nullptr;
	for (TActorIterator<AGolemStoryEntry> It(World); It; ++It)
	{
		Entry = *It;
	}
	if (Entry == nullptr)
	{
		Entry = World->SpawnActor<AGolemStoryEntry>();
	}
	const FName ProfileName = Milestone == 61 ? TEXT("Caravan") : Milestone == 62 ? TEXT("Lifecycle")
		: Milestone == 63 ? TEXT("Demo_M6_V1") : TEXT("Demo_M6_V2");
	Entry->ProfileId = FPrimaryAssetId(TEXT("KnightStoryProfile"), ProfileName);
	const FString WorldName = Milestone == 61 ? TEXT("Caravan") : Milestone == 62 ? TEXT("Lifecycle") : TEXT("ForgeSchool");
	UGolemStoryWorldDefinition* Definition = LoadObject<UGolemStoryWorldDefinition>(nullptr,
		*(Root + TEXT("DA_Knight_World_") + WorldName + TEXT(".DA_Knight_World_") + WorldName));
	if (Definition == nullptr)
	{
		return false;
	}
	for (int32 Index = 0; Index < Definition->Interactions.Num(); ++Index)
	{
		const FGolemStoryInteraction& Row = Definition->Interactions[Index];
		AGolemStoryAnchor* Anchor = nullptr;
		for (TActorIterator<AGolemStoryAnchor> It(World); It; ++It)
		{
			if (It->AnchorId == Row.AnchorId)
			{
				Anchor = *It;
				break;
			}
		}
		if (Anchor == nullptr)
		{
			Anchor = World->SpawnActor<AGolemStoryAnchor>();
		}
		if (Anchor == nullptr)
		{
			return false;
		}
		Anchor->AnchorId = Row.AnchorId;
		Anchor->Label = Row.Label;
		Anchor->SetActorLocation(FVector(-1500.0 + (Index % 7) * 450.0, -1300.0 + (Index / 7) * 700.0, 60.0));
		Anchor->RefreshPresentation();
	}
	if (Milestone == 61)
	{
		AGolemProtectedCaravan* ProtectedTarget = nullptr;
		for (TActorIterator<AGolemProtectedCaravan> It(World); It; ++It)
		{
			ProtectedTarget = *It;
		}
		if (ProtectedTarget == nullptr)
		{
			ProtectedTarget = World->SpawnActor<AGolemProtectedCaravan>();
		}
		if (ProtectedTarget == nullptr)
		{
			return false;
		}
		// Keep the protected target outside the inherited M4 LOS blocker, on reachable navigation.
		ProtectedTarget->SetActorLocation(FVector(0.0, 600.0, 80.0));
		if (UStaticMeshComponent* Body = ProtectedTarget->FindComponentByClass<UStaticMeshComponent>())
		{
			Body->SetCanEverAffectNavigation(false);
		}
	}
	return true;
}
