#include "Story/GolemStoryGameMode.h"

#include "Assets/GolemAssetManager.h"
#include "Attributes/GolemAttributeSet.h"
#include "Character/GolemCharacter.h"
#include "Checkpoint/GolemCheckpointSubsystem.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Controllers/GolemEnemyController.h"
#include "Context/GolemAIContextComponent.h"
#include "PartyArena/GolemArenaEnemy.h"
#include "Threat/GolemThreatComponent.h"
#include "Story/GolemProtectedCaravan.h"
#include "Catalog/GolemGearCatalog.h"
#include "Definitions/GolemConversationDefinition.h"
#include "Definitions/GolemItemDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/GolemItemEvent.h"
#include "Events/GolemSkillLabEvent.h"
#include "Interaction/GolemStoryAnchor.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"
#include "Party/GolemPartyState.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "Story/GolemStoryPlayerController.h"
#include "Story/GolemStoryHUD.h"
#include "Story/GolemStoryProfile.h"
#include "Story/GolemStoryEntry.h"
#include "Story/GolemStoryState.h"
#include "SkillLab/GolemRunestone.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "Tags/GolemGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemStory, Log, All);

namespace
{
	template <typename TDefinition>
	FGolemResult ResolveStoryDefinition(const FPrimaryAssetId& Id, TDefinition*& OutDefinition)
	{
		UGolemDefinition* Resolved = nullptr;
		UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
		const FGolemResult Result = Manager != nullptr
			? Manager->ResolveDefinition(Id, TDefinition::StaticClass(), Resolved)
			: FGolemResult::Failure(EGolemError::LoadFailed, TEXT("StoryCatalog"), FText::GetEmpty());
		OutDefinition = Cast<TDefinition>(Resolved);
		return Result;
	}

	FGuid StoryReceipt(const FGuid& RunId, const FName Key)
	{
		const FString Digest = FMD5::HashAnsiString(*(RunId.ToString(EGuidFormats::Digits) + Key.ToString()));
		FGuid Result;
		FGuid::ParseExact(Digest, EGuidFormats::Digits, Result);
		return Result;
	}
}

AGolemStoryGameMode::AGolemStoryGameMode()
{
	GameStateClass = AGolemStoryState::StaticClass();
	PlayerControllerClass = AGolemStoryPlayerController::StaticClass();
	HUDClass = AGolemStoryHUD::StaticClass();
	InitializationResult = FGolemResult::Failure(EGolemError::InvalidState, TEXT("StoryPending"), FText::GetEmpty());
}

FGolemResult AGolemStoryGameMode::ResolveProfile()
{
	UGolemStoryProfile* ResolvedProfile = nullptr;
	FGolemResult Result = ResolveStoryDefinition(ProfileId, ResolvedProfile);
	Profile = ResolvedProfile;
#if UE_BUILD_SHIPPING
	if (Result.IsSuccess() && Profile->bDeveloperFixture)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("DeveloperScenario"), FText::GetEmpty());
	}
#endif
	if (Result.IsSuccess())
	{
		UGolemCampaignDefinition* ResolvedCampaign = nullptr;
		Result = ResolveStoryDefinition(Profile->CampaignDefinitionId, ResolvedCampaign);
		Campaign = ResolvedCampaign;
	}
	if (Result.IsSuccess())
	{
		Result = Campaign->ValidateCampaign();
	}
	if (Result.IsSuccess())
	{
		UGolemStoryWorldDefinition* ResolvedWorld = nullptr;
		Result = ResolveStoryDefinition(Profile->WorldDefinitionId, ResolvedWorld);
		WorldDefinition = ResolvedWorld;
	}
	if (Result.IsSuccess())
	{
		Result = WorldDefinition->ValidateWorld();
	}
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if ((!Profile->bDeveloperFixture && (!Profile->DeveloperSeedItems.IsEmpty() || !Profile->DeveloperBuilds.IsEmpty()
		|| Profile->DeveloperLevel != 1 || Profile->bDeveloperStarterEquipment))
		|| Profile->DeveloperLevel < 1 || Profile->DeveloperLevel > 100)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("ProductionSeed"), FText::GetEmpty());
	}
	TSet<FName> RewardQuests;
	for (const FGolemStoryReward& Reward : Profile->Rewards)
	{
		const FGolemQuestDefinition* Quest = Campaign->FindQuest(Reward.QuestId);
		if (Quest == nullptr || Quest->RewardKey != Reward.RewardKey || RewardQuests.Contains(Reward.QuestId)
			|| Reward.Items.IsEmpty() || Reward.Currency < 0)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, Reward.QuestId, FText::GetEmpty());
		}
		RewardQuests.Add(Reward.QuestId);
		for (const FGolemGearRewardItem& Item : Reward.Items)
		{
			UGolemItemDefinition* ItemDefinition = nullptr;
			Result = ResolveStoryDefinition(Item.DefinitionId, ItemDefinition);
			if (!Result.IsSuccess() || !ItemDefinition->ValidateItem().IsSuccess() || Item.Count < 1 || Item.Count > 10000)
			{
				return FGolemResult::Failure(EGolemError::InvalidData, FName(*Item.DefinitionId.ToString()), FText::GetEmpty());
			}
		}
	}
	for (const FGolemStoryInteraction& Interaction : WorldDefinition->Interactions)
	{
		if (!Interaction.RequiredQuestId.IsNone())
		{
			const FGolemQuestDefinition* Quest = Campaign->FindQuest(Interaction.RequiredQuestId);
			if (Quest == nullptr || !Interaction.RequiredObjectiveId.IsNone()
				&& !Quest->Objectives.ContainsByPredicate([&Interaction](const FGolemObjectiveDefinition& Objective)
				{
					return Objective.ObjectiveId == Interaction.RequiredObjectiveId;
				}))
			{
				return FGolemResult::Failure(EGolemError::NotFound, Interaction.RequiredQuestId, FText::GetEmpty());
			}
		}
		if (Interaction.Action == EGolemStoryAction::Conversation)
		{
			UGolemConversationDefinition* Conversation = nullptr;
			Result = ResolveStoryDefinition(Interaction.ConversationId, Conversation);
			if (!Result.IsSuccess() || !Conversation->ValidateConversation().IsSuccess())
			{
				return FGolemResult::Failure(EGolemError::NotFound,
					FName(*Interaction.ConversationId.ToString()), FText::GetEmpty());
			}
		}
		if (Interaction.Action == EGolemStoryAction::Reward && !RewardQuests.Contains(Interaction.RewardQuestId))
		{
			return FGolemResult::Failure(EGolemError::NotFound, Interaction.RewardQuestId, FText::GetEmpty());
		}
	}
	return Result;
}

void AGolemStoryGameMode::InitGameState()
{
	for (TActorIterator<AGolemStoryEntry> It(GetWorld()); It; ++It)
	{
		ProfileId = It->ProfileId;
		break;
	}
	InitializationResult = ResolveProfile();
	UGolemCheckpointSubsystem* Save = GetGameInstance()->GetSubsystem<UGolemCheckpointSubsystem>();
	FString CheckpointFixture;
#if !UE_BUILD_SHIPPING
	FParse::Value(FCommandLine::Get(), TEXT("GolemCheckpointFixture="), CheckpointFixture);
#endif
	if (InitializationResult.IsSuccess()
		&& (FParse::Param(FCommandLine::Get(), TEXT("GolemContinue")) || !CheckpointFixture.IsEmpty()))
	{
		FGolemCampaignCheckpoint Loaded;
		InitializationResult = CheckpointFixture.IsEmpty()
			? Save->LoadCheckpoint(Campaign->CampaignId, Campaign->ScenarioKind, Loaded)
			: UGolemCheckpointSubsystem::ReadCheckpointFile(CheckpointFixture, Loaded);
		if (InitializationResult.IsSuccess())
		{
			InitializationResult = Save->StageCheckpoint(Loaded);
		}
	}
	const FGolemCampaignCheckpoint* Staged = Save->GetStagedCheckpoint();
	if (InitializationResult.IsSuccess() && Staged != nullptr
		&& (Staged->Quest.CampaignId != Campaign->CampaignId || Staged->Quest.ScenarioKind != Campaign->ScenarioKind))
	{
		InitializationResult = FGolemResult::Failure(EGolemError::Conflict, TEXT("ScenarioCheckpoint"), FText::GetEmpty());
	}
	RunId = Staged != nullptr ? Staged->Quest.RunId : FGuid::NewGuid();
	Super::InitGameState();
	Story = GetGameState<AGolemStoryState>();
	if (!InitializationResult.IsSuccess() || !GetRPGInitializationResult().IsSuccess() || Story == nullptr)
	{
		return;
	}
	InitializationResult = Story->Quest->InitializeCampaign(Campaign, RunId);
	if (InitializationResult.IsSuccess())
	{
		InitializationResult = Story->WorldState->InitializeWorld(Campaign->CampaignId, RunId);
		Story->SetWorldDefinitionId(WorldDefinition->GetPrimaryAssetId());
	}
	if (InitializationResult.IsSuccess() && Staged != nullptr)
	{
		InitializationResult = Story->Quest->ValidateSnapshot(Staged->Quest);
		if (InitializationResult.IsSuccess())
		{
			InitializationResult = Story->WorldState->ValidateSnapshot(Staged->World);
		}
		if (InitializationResult.IsSuccess())
		{
			InitializationResult = Story->Quest->RestoreSnapshot(Staged->Quest);
		}
		if (InitializationResult.IsSuccess())
		{
			InitializationResult = Story->WorldState->RestoreSnapshot(Staged->World);
		}
		if (InitializationResult.IsSuccess())
		{
			InitializationResult = Story->Dialogue->RestoreChoices(Staged->Dialogue);
		}
		if (InitializationResult.IsSuccess())
		{
			for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
			{
				InitializationResult = Entry.Golem->GetCombatComponent()->RestoreSnapshotPoise(
					Staged->Poise.FindChecked(Entry.GolemId));
				if (!InitializationResult.IsSuccess())
				{
					break;
				}
			}
		}
		if (InitializationResult.IsSuccess())
		{
			Save->ConsumeStagedCheckpoint();
		}
	}
}

FGolemEntityId AGolemStoryGameMode::CreateGolemIdentity(const int32 Slot, const FPrimaryAssetId& HeroId) const
{
	const FGolemCampaignCheckpoint* Staged =
		GetGameInstance()->GetSubsystem<UGolemCheckpointSubsystem>()->GetStagedCheckpoint();
	if (Staged != nullptr)
	{
		const FGolemRPGGolemView* Golem = Staged->RPG.View.Golems.FindByPredicate(
			[HeroId](const FGolemRPGGolemView& Row)
			{
				return Row.HeroDefinitionId == HeroId;
			});
		return Golem != nullptr ? Golem->GolemId : FGolemEntityId();
	}
	return Super::CreateGolemIdentity(Slot, HeroId);
}

FGolemResult AGolemStoryGameMode::InitializeRPGDomains(UGolemInventoryComponent* Inventory,
	UGolemRPGComponent* RPG, TConstArrayView<FGolemRPGGolemRegistration> Registrations)
{
	if (!InitializationResult.IsSuccess())
	{
		return InitializationResult;
	}
	const FGolemCampaignCheckpoint* Staged =
		GetGameInstance()->GetSubsystem<UGolemCheckpointSubsystem>()->GetStagedCheckpoint();
	return Staged != nullptr ? RPG->TryRestoreSnapshot(Inventory, Staged->RPG, Registrations)
		: Super::InitializeRPGDomains(Inventory, RPG, Registrations);
}

FGolemResult AGolemStoryGameMode::ConfigureRPGDomains(UGolemInventoryComponent* Inventory, UGolemRPGComponent* RPG)
{
	if (Profile->bDeveloperFixture)
	{
		FGolemInventoryRequest Request;
		Request.RequestId = FGuid::NewGuid();
		Request.ExpectedRevision = Inventory->GetView().Revision;
		for (const FGolemGearRewardItem& Seed : Profile->DeveloperSeedItems)
		{
			UGolemItemDefinition* Definition = nullptr;
			FGolemResult Resolved = ResolveStoryDefinition(Seed.DefinitionId, Definition);
			if (!Resolved.IsSuccess() || Seed.Count < 1 || Seed.Count > 10000)
			{
				return FGolemResult::Failure(EGolemError::InvalidData, TEXT("FixtureSeed"), FText::GetEmpty());
			}
			int32 Remaining = Seed.Count;
			while (Remaining > 0)
			{
				FGolemInventoryMutation& Mutation = Request.Mutations.AddDefaulted_GetRef();
				Mutation.Item.InstanceId = FGuid::NewGuid();
				Mutation.Item.DefinitionId = Seed.DefinitionId;
				Mutation.Item.Count = FMath::Min(Remaining, Definition->GetMaximumStack());
				Remaining -= Mutation.Item.Count;
			}
		}
		const FGolemResult Seed = Inventory->TryApplyTransaction(Request).Result;
		if (!Seed.IsSuccess())
		{
			return Seed;
		}
	}
	// Demo profile starts at level one, zero currency and an empty bag. No CatalogLab unlock/starter seed.
	if (Profile->bDeveloperFixture && !Profile->DeveloperBuilds.IsEmpty())
	{
		const FGolemResult Builds = RPG->ConfigureSkillLabBuilds(Profile->DeveloperBuilds);
		if (!Builds.IsSuccess())
		{
			return Builds;
		}
	}
	return RPG->ConfigureGearDemo(RunId, 0, Profile->bDeveloperFixture ? Profile->DeveloperLevel : 1,
		Profile->bDeveloperFixture && Profile->bDeveloperStarterEquipment);
}

bool AGolemStoryGameMode::ShouldIssueLegacyEncounterRewards() const
{
	return false;
}

bool AGolemStoryGameMode::ShouldAdvanceWaveToBoss() const
{
	return false;
}

FGolemResult AGolemStoryGameMode::BuildCaller(APlayerController* Player, FGolemRPGCallerContext& OutCaller) const
{
	FGolemResult Result = Super::BuildCaller(Player, OutCaller);
	if (Result.IsSuccess() && InitializationResult.IsSuccess() && Story != nullptr)
	{
		// School is the authored non-combat sanctuary; no lab reward/level bypass is involved.
		OutCaller.bInSafeZone = IsInUpgradeSafeZone(OutCaller.Pawn.Get());
		OutCaller.EncounterId = Story->WorldState->GetSnapshot().EncounterRunId;
	}
	return Result;
}

bool AGolemStoryGameMode::IsInUpgradeSafeZone(const AActor* Golem) const
{
	const UGolemLifeComponent* Life = IsValid(Golem) ? Golem->FindComponentByClass<UGolemLifeComponent>() : nullptr;
	return InitializationResult.IsSuccess() && Story != nullptr && Life != nullptr && Life->IsAlive()
		&& Golem->GetWorld() == GetWorld() && GetArenaState()->GetSnapshot().AliveEnemies == 0
		&& Story->WorldState->GetSnapshot().EncounterPhase != EGolemStoryEncounterPhase::Active
		&& !Story->WorldState->GetSnapshot().bSceneActive;
}

void AGolemStoryGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (!InitializationResult.IsSuccess() || !GetRPGInitializationResult().IsSuccess())
	{
		UE_LOG(LogGolemStory, Error, TEXT("Story composition unavailable: %s / %s"),
			*InitializationResult.Context.ToString(), *GetRPGInitializationResult().Context.ToString());
		return;
	}
	for (TActorIterator<AGolemStoryAnchor> It(GetWorld()); It; ++It)
	{
		InitializationResult = Story->WorldState->BindAnchor(*It);
		if (!InitializationResult.IsSuccess())
		{
			UE_LOG(LogGolemStory, Error, TEXT("Duplicate/missing story anchor %s"), *It->AnchorId.ToString());
			return;
		}
	}
	QuestHandle = Story->Quest->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleQuestChanged);
	ArenaHandle = GetArenaState()->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleArenaChanged);
	WorldHandle = Story->WorldState->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleWorldChanged);
	for (TActorIterator<AGolemProtectedCaravan> It(GetWorld()); It; ++It)
	{
		Caravan = *It;
		CaravanHandle = Caravan->GetLifeComponent()->OnLifeStateChanged.AddUObject(this, &ThisClass::HandleCaravanLifeChanged);
	}
	UGolemEventSubsystem* Bus = GetWorld()->GetSubsystem<UGolemEventSubsystem>();
	ItemSubscription = Bus->Subscribe<FGolemItemEvent>(this, [this](const FGolemItemEvent& Event)
	{
		HandleItemFact(Event);
	});
	SkillSubscription = Bus->Subscribe<FGolemSkillLabEvent>(this, [this](const FGolemSkillLabEvent& Event)
	{
		HandleSkillFact(Event);
	});
	HandleQuestChanged();
	UE_LOG(LogGolemStory, Display, TEXT("StoryReady campaign=%s run=%s content=%d canonical=3"),
		*Campaign->CampaignId.ToString(), *RunId.ToString(), Campaign->ContentRevision);
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("GolemStoryColdSmoke")))
	{
		const bool bPending = FParse::Param(FCommandLine::Get(), TEXT("GolemStoryPendingSmoke"));
		const FGolemQuestProgress* Append = Story->Quest->FindProgress(TEXT("Demo_Append"));
		bool bPassed = Campaign->ContentRevision == 2
			&& Story->Quest->FindProgress(TEXT("ActI_Q1")) != nullptr
			&& Story->Quest->FindProgress(TEXT("ActI_Q1"))->Phase == EGolemQuestPhase::Complete
			&& Story->Quest->FindProgress(TEXT("ActI_Q2")) != nullptr
			&& Story->Quest->FindProgress(TEXT("ActI_Q2"))->Phase ==
				(bPending ? EGolemQuestPhase::RewardPending : EGolemQuestPhase::Complete)
			&& GetRPGState()->GetInventory()->GetView().Items.Num() == (bPending ? 0 : 3)
			&& GetRPGState()->GetRPG()->GetView().ClaimedRewards.Num() == (bPending ? 0 : 1)
			&& Append != nullptr && Append->Phase == (bPending ? EGolemQuestPhase::Locked : EGolemQuestPhase::Available);
		if (bPassed && FParse::Param(FCommandLine::Get(), TEXT("GolemStoryColdResave")))
		{
			bPassed = SaveStoryCheckpoint().IsSuccess();
		}
		const FString Evidence = FString::Printf(TEXT("result=%s campaign=%s run=%s content=%d receipts=%d items=%d appendedAvailable=%d pendingExpected=%d\n"),
			bPassed ? TEXT("PASS") : TEXT("FAIL"), *Campaign->CampaignId.ToString(), *RunId.ToString(),
			Campaign->ContentRevision, GetRPGState()->GetRPG()->GetView().ClaimedRewards.Num(),
			GetRPGState()->GetInventory()->GetView().Items.Num(), Append != nullptr && Append->Phase == EGolemQuestPhase::Available, bPending);
		UE_LOG(LogGolemStory, Display, TEXT("ColdProcessProof %s"), *Evidence);
		FFileHelper::SaveStringToFile(Evidence, *(FPaths::ProjectSavedDir() / TEXT("M6ColdProcessProof.txt")));
		FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
	}
#endif
}

void AGolemStoryGameMode::HandleQuestChanged()
{
	if (Story == nullptr || bStoryMutation || GetRPGState()->GetRPG()->IsTransactionInProgress())
	{
		return;
	}
	for (const FGolemStoryReward& Reward : Profile->Rewards)
	{
		const FGolemQuestProgress* Progress = Story->Quest->FindProgress(Reward.QuestId);
		if (Progress == nullptr || Progress->Phase != EGolemQuestPhase::RewardPending)
		{
			continue;
		}
		FGolemGearRewardBundle Bundle;
		Bundle.RewardId = StoryReceipt(RunId, Reward.RewardKey);
		Bundle.DemoRunId = RunId;
		Bundle.EncounterId = Story->WorldState->GetSnapshot().EncounterRunId;
		Bundle.Currency = Reward.Currency;
		Bundle.Level = 1;
		Bundle.Items = Reward.Items;
		const TArray<FGolemGearRewardBundle> ExistingOffers = GetRPGState()->GetRPG()->GetRewardOffers();
		if (ExistingOffers.ContainsByPredicate([&Bundle](const FGolemGearRewardBundle& Existing)
		{
			return Existing.RewardId == Bundle.RewardId;
		}))
		{
			continue;
		}
		if (Reward.bReuseExistingHammers)
		{
			Bundle.Items.Reset();
			const FGolemRPGView PartyView = GetRPGState()->GetRPG()->GetView();
			for (int32 Index = 0; Index < PartyView.Golems.Num(); ++Index)
			{
				const FGolemRPGGolemView& Golem = PartyView.Golems[Index];
				const FGolemEquipmentAssignment* Weapon = Golem.FindAssignment(GolemGameplayTags::Slot_Equipment_Weapon);
				if (Weapon != nullptr)
				{
					// Bind existing physical hammers unchanged, including their GUIDs and attached Runes.
					Bundle.BoundItems.Add(Weapon->ItemInstanceId);
				}
				else if (Reward.Items.IsValidIndex(Index))
				{
					Bundle.Items.Add(Reward.Items[Index]);
				}
			}
		}
		if (Bundle.Items.IsEmpty() && Bundle.BoundItems.IsEmpty())
		{
			continue;
		}
		const FGolemResult Registered = GetRPGState()->GetRPG()->RegisterRewardBundle(Bundle);
		if (!Registered.IsSuccess())
		{
			UE_LOG(LogGolemStory, Error, TEXT("Story reward offer failed: %s"), *Registered.Context.ToString());
		}
	}
}

FGolemResult AGolemStoryGameMode::ClaimQuestReward(APlayerController* Player, const FName QuestId)
{
	const FGolemQuestProgress* Progress = Story->Quest->FindProgress(QuestId);
	const FGolemStoryReward* Reward = Profile->Rewards.FindByPredicate([QuestId](const FGolemStoryReward& Row)
	{
		return Row.QuestId == QuestId;
	});
	if (Progress == nullptr || Reward == nullptr || Progress->Phase != EGolemQuestPhase::RewardPending)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, QuestId,
			NSLOCTEXT("GolemStory", "RewardUnavailable", "This quest has no pending reward."));
	}
	HandleQuestChanged();
	FGolemRPGCallerContext Caller;
	FGolemResult Result = BuildCaller(Player, Caller);
	if (!Result.IsSuccess() || !Player->IsLocalController())
	{
		return Result.IsSuccess() ? FGolemResult::Failure(EGolemError::NotOwned,
			QuestId, FText::GetEmpty()) : Result;
	}
	const FGuid Receipt = StoryReceipt(RunId, Reward->RewardKey);
	if (GetRPGState()->GetRPG()->GetView().ClaimedRewards.Contains(Receipt))
	{
		return Story->Quest->CommitReward(QuestId, Receipt);
	}
	FGolemRPGCommand Command;
	Command.Operation = EGolemRPGOperation::ClaimReward;
	Command.RequestId = Receipt;
	Command.ExpectedGolemId = Caller.GolemId;
	Command.InventoryId = GetRPGState()->GetInventory()->GetView().InventoryId;
	Command.ExpectedInventoryRevision = GetRPGState()->GetInventory()->GetView().Revision;
	Command.ExpectedGolemRevision = GetRPGState()->GetRPG()->GetGolemView(Caller.GolemId).Revision;
	Command.RewardId = Receipt;
	Result = GetRPGState()->GetRPG()->ExecuteCommand(Caller, Command).Result;
	return Result.IsSuccess() ? Story->Quest->CommitReward(QuestId, Receipt) : Result;
}

FGolemResult AGolemStoryGameMode::InteractAnchor(APlayerController* Player, const FName AnchorId,
	const FGuid& RequestId)
{
	return InteractAnchor(Player, AnchorId, RequestId, NAME_None);
}

FGolemResult AGolemStoryGameMode::InteractAnchor(APlayerController* Player, const FName AnchorId,
	const FGuid& RequestId, const FName ChoiceId)
{
	const FGolemStoryInteraction* Authored = WorldDefinition->FindInteraction(AnchorId);
	if (Authored == nullptr)
	{
		return FGolemResult::Failure(EGolemError::NotFound, AnchorId, FText::GetEmpty());
	}
	FGolemStoryInteraction Resolved = *Authored;
	if (!ChoiceId.IsNone())
	{
		if (!Resolved.AllowedChoices.Contains(ChoiceId))
		{
			return FGolemResult::Failure(EGolemError::InvalidId, ChoiceId, FText::GetEmpty());
		}
		Resolved.ValueId = ChoiceId;
	}
	const FGolemStoryInteraction* Interaction = &Resolved;
	const FName* BranchChoice = Story->Quest->GetSnapshot().Branches.Find(Interaction->RequiredBranchId);
	if (!Interaction->RequiredBranchId.IsNone()
		&& (BranchChoice == nullptr || *BranchChoice != Interaction->RequiredBranchChoice))
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("BranchGate"),
			NSLOCTEXT("GolemStory", "BranchGate", "This route is closed by the retained campaign choice."));
	}
	const FGolemQuestProgress* Progress = Interaction->RequiredQuestId.IsNone()
		? nullptr : Story->Quest->FindProgress(Interaction->RequiredQuestId);
	if (!Interaction->RequiredQuestId.IsNone() && (Progress == nullptr || Progress->Phase != EGolemQuestPhase::Active))
	{
		return FGolemResult::Failure(EGolemError::InvalidState, Interaction->RequiredQuestId,
			NSLOCTEXT("GolemStory", "QuestLocked", "Follow the current lesson before using this station."));
	}
	if (!Interaction->RequiredObjectiveId.IsNone())
	{
		const FGolemQuestDefinition* Definition = Campaign->FindQuest(Progress->QuestId);
		FName NextObjective;
		for (const FGolemObjectiveDefinition& Objective : Definition->Objectives)
		{
			if (Progress->Counts.FindChecked(Objective.ObjectiveId) < Objective.RequiredCount)
			{
				NextObjective = Objective.ObjectiveId;
				break;
			}
		}
		if (NextObjective != Interaction->RequiredObjectiveId)
		{
			return FGolemResult::Failure(EGolemError::InvalidState, Interaction->RequiredObjectiveId,
				NSLOCTEXT("GolemStory", "ForgeOrder", "Use the forge stations in the displayed order."));
		}
	}
	FGuid Token;
	FGolemResult Result = Story->WorldState->ReserveInteraction(AnchorId, Player->GetPawn(), RequestId,
		Story->WorldState->GetSnapshot().Revision, Interaction->InteractionRange, Token);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	ON_SCOPE_EXIT
	{
		Story->WorldState->ReleaseReservation(Token);
	};
	Result = Story->WorldState->ValidateReservation(Token);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	switch (Interaction->Action)
	{
	case EGolemStoryAction::Branch:
		if (!Player->IsLocalController())
		{
			return FGolemResult::Failure(EGolemError::NotOwned, TEXT("HostBranch"), FText::GetEmpty());
		}
		Result = Story->Quest->CommitBranch(Interaction->SubjectId, Interaction->ValueId,
			Story->Quest->GetSnapshot().Revision);
		return Result.IsSuccess() ? Story->WorldState->CommitFact(TEXT("BranchCommitted"),
			Interaction->SubjectId, Interaction->ValueId, true, RequestId) : Result;
	case EGolemStoryAction::Conversation:
	{
		if (!Player->IsLocalController())
		{
			return FGolemResult::Failure(EGolemError::NotOwned, TEXT("ConversationHost"), FText::GetEmpty());
		}
		UGolemConversationDefinition* Conversation = nullptr;
		Result = ResolveStoryDefinition(Interaction->ConversationId, Conversation);
		return Result.IsSuccess() ? Story->Dialogue->StartConversation(Conversation) : Result;
	}
	case EGolemStoryAction::Reward:
		Story->WorldState->ReleaseReservation(Token);
		return ClaimQuestReward(Player, Interaction->RewardQuestId);
	case EGolemStoryAction::Checkpoint:
		Story->WorldState->ReleaseReservation(Token);
		if (Player->IsLocalController())
		{
			Story->WorldState->SetCheckpoint(AnchorId);
		}
		return Player->IsLocalController() ? SaveStoryCheckpoint()
			: FGolemResult::Failure(EGolemError::NotOwned, TEXT("HostCheckpoint"), FText::GetEmpty());
	case EGolemStoryAction::Travel:
		Story->WorldState->ReleaseReservation(Token);
		return TravelStory(Player, Interaction->Destination.ToSoftObjectPath().GetLongPackageName());
	case EGolemStoryAction::Encounter:
		Result = Story->WorldState->BeginEncounter(Interaction->SubjectId);
		if (Result.IsSuccess())
		{
			Story->Quest->SetEncounterRun(Story->WorldState->GetSnapshot().EncounterRunId);
			Result = StartFixtureEncounter(EGolemArenaStage::Wave);
			if (Result.IsSuccess())
			{
				const FGuid EncounterRun = Story->WorldState->GetSnapshot().EncounterRunId;
				for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
				{
					if (UGolemSkillRuntimeComponent* Runtime = Entry.Golem->FindComponentByClass<UGolemSkillRuntimeComponent>())
					{
						Runtime->SetEncounterId(EncounterRun);
					}
				}
				for (TActorIterator<AGolemRunestone> It(GetWorld()); It; ++It)
				{
					It->ResetFixture(EncounterRun);
				}
			}
			if (Result.IsSuccess() && Caravan != nullptr)
			{
				for (AGolemArenaEnemy* Enemy : GetEncounterEnemies())
				{
					Enemy->GetThreatComponent()->AddThreat(Caravan, 10000.0f);
					if (AGolemEnemyController* Controller = Cast<AGolemEnemyController>(Enemy->GetController()))
					{
						Controller->GetAIContext()->SetPerceivedActor(Caravan, true);
					}
				}
			}
		}
		return Result;
	case EGolemStoryAction::Scene:
	{
		Story->WorldState->ReleaseReservation(Token);
		FGolemCampaignCheckpoint Boundary;
		Result = CaptureCheckpoint(Boundary);
		return Result.IsSuccess() && Player->IsLocalController() ? Story->WorldState->BeginScene(Interaction->SubjectId)
			: Result.IsSuccess() ? FGolemResult::Failure(EGolemError::NotOwned, TEXT("HostScene"), FText::GetEmpty()) : Result;
	}
	case EGolemStoryAction::Deliver:
	{
		if (!Player->IsLocalController())
		{
			return FGolemResult::Failure(EGolemError::NotOwned, TEXT("HostDelivery"),
				NSLOCTEXT("GolemStory", "HostDelivery", "The host confirms shared quest deliveries."));
		}
		FGolemRPGCallerContext Caller;
		Result = BuildCaller(Player, Caller);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		const FGolemInventoryView Bag = GetRPGState()->GetInventory()->GetView();
		const FGolemItemInstance* Stack = Bag.Items.FindByPredicate([Interaction](const FGolemItemInstance& Item)
		{
			return Item.DefinitionId == Interaction->DeliveryItemId && Item.Count >= Interaction->DeliveryCount
				&& Item.LocationId == GolemGear::InventoryLocation;
		});
		if (Stack == nullptr)
		{
			return FGolemResult::Failure(EGolemError::InsufficientResources, TEXT("QuestDelivery"),
				NSLOCTEXT("GolemStory", "DeliveryMissing", "Bring the required item before delivering it."));
		}
		FGolemRPGCommand Command;
		Command.Operation = EGolemRPGOperation::DeliverQuestItem;
		Command.RequestId = RequestId;
		Command.ExpectedGolemId = Caller.GolemId;
		Command.InventoryId = Bag.InventoryId;
		Command.ExpectedInventoryRevision = Bag.Revision;
		Command.ExpectedGolemRevision = GetRPGState()->GetRPG()->GetGolemView(Caller.GolemId).Revision;
		Command.ItemInstanceId = Stack->InstanceId;
		Command.ExpectedItemRevision = Stack->ItemRevision;
		Command.ItemDefinitionId = Interaction->DeliveryItemId;
		Command.Quantity = Interaction->DeliveryCount;
		Caller.bQuestDeliveryAuthorized = true;
		return GetRPGState()->GetRPG()->ExecuteCommand(Caller, Command).Result;
	}
	case EGolemStoryAction::Forge:
	case EGolemStoryAction::Fact:
	{
		{
			TGuardValue<bool> MutationGuard(bStoryMutation, true);
			Result = Story->WorldState->CommitFact(Interaction->EventName, Interaction->SubjectId, Interaction->ValueId);
		}
		HandleQuestChanged();
		return Result;
	}
	}
	return FGolemResult::Failure(EGolemError::InvalidData, AnchorId, FText::GetEmpty());
}

FGolemResult AGolemStoryGameMode::TryInteract(APlayerController* Player)
{
	FGolemRPGCallerContext Caller;
	FGolemResult Result = BuildCaller(Player, Caller);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (Story->Dialogue->GetSnapshot().bActive)
	{
		bool bFinished = false;
		const FName NpcId = Story->Dialogue->GetSnapshot().NpcId;
		Result = Story->Dialogue->AdvanceConversation(Player->IsLocalController(),
			Story->Dialogue->GetSnapshot().Revision, NAME_None, bFinished);
		if (Result.IsSuccess() && bFinished)
		{
			Result = Story->WorldState->CommitFact(TEXT("ConversationFinished"), NpcId);
		}
		return Result;
	}
	AGolemStoryAnchor* Nearest = nullptr;
	double Distance = TNumericLimits<double>::Max();
	for (TActorIterator<AGolemStoryAnchor> It(GetWorld()); It; ++It)
	{
		const FGolemStoryInteraction* Interaction = WorldDefinition->FindInteraction(It->AnchorId);
		const double Candidate = FVector::DistSquared(Player->GetPawn()->GetActorLocation(), It->GetActorLocation());
		if (Interaction != nullptr && Candidate <= FMath::Square(Interaction->InteractionRange) && Candidate < Distance)
		{
			Nearest = *It;
			Distance = Candidate;
		}
	}
	return Nearest != nullptr ? InteractAnchor(Player, Nearest->AnchorId, FGuid::NewGuid())
		: FGolemResult::Failure(EGolemError::OutOfRange, TEXT("StoryStation"),
			NSLOCTEXT("GolemStory", "FindStation", "Move close to a labelled station and use Interact."));
}

FGolemResult AGolemStoryGameMode::ExecuteStoryAction(APlayerController* Player, const FName Action,
	const FName AnchorId, const FName ChoiceId, const FGolemEntityId& ExpectedGolem,
	const FGuid& RequestId, const int64 ExpectedRevision)
{
	FGolemRPGCallerContext Caller;
	FGolemResult Result = BuildCaller(Player, Caller);
	if (!Result.IsSuccess() || Caller.GolemId != ExpectedGolem || !RequestId.IsValid()
		|| bTravelling || bStoryMutation)
	{
		return Result.IsSuccess() ? FGolemResult::Failure(EGolemError::InvalidState,
			TEXT("StoryCommand"), FText::GetEmpty()) : Result;
	}
	if (Action == TEXT("Interact"))
	{
		return Story->WorldState->GetSnapshot().Revision == ExpectedRevision
			? InteractAnchor(Player, AnchorId, RequestId, ChoiceId)
			: FGolemResult::Failure(EGolemError::InvalidRevision, AnchorId, FText::GetEmpty());
	}
	if (!Player->IsLocalController())
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("HostDecision"), FText::GetEmpty());
	}
	if (Action == TEXT("Advance"))
	{
		bool bFinished = false;
		const FName NpcId = Story->Dialogue->GetSnapshot().NpcId;
		Result = Story->Dialogue->AdvanceConversation(true, ExpectedRevision, ChoiceId, bFinished);
		return Result.IsSuccess() && bFinished ? Story->WorldState->CommitFact(TEXT("ConversationFinished"), NpcId) : Result;
	}
	if (Action == TEXT("Cancel"))
	{
		Story->Dialogue->CancelConversation();
		if (Story->WorldState->GetSnapshot().bSceneActive)
		{
			Result = Story->WorldState->FinishScene(TEXT("Cancelled"), Story->WorldState->GetSnapshot().SceneRunId);
		}
		Story->WorldState->ReleasePawnReservations(Player->GetPawn());
		return Result;
	}
	if (Action == TEXT("Skip"))
	{
		return Story->WorldState->FinishScene(TEXT("Skipped"), Story->WorldState->GetSnapshot().SceneRunId);
	}
	if (Action == TEXT("Save"))
	{
		return SaveStoryCheckpoint();
	}
	if (Action == TEXT("New"))
	{
		FGolemCampaignCheckpoint Boundary;
		Result = CaptureCheckpoint(Boundary);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		GetGameInstance()->GetSubsystem<UGolemCheckpointSubsystem>()->ConsumeStagedCheckpoint();
		bTravelling = GetWorld()->ServerTravel(Boundary.MapPackage);
		return bTravelling ? FGolemResult::Success()
			: FGolemResult::Failure(EGolemError::LoadFailed, TEXT("NewCampaignTravel"), FText::GetEmpty());
	}
	if (Action == TEXT("Continue"))
	{
		FGolemCampaignCheckpoint SafeBoundary;
		Result = CaptureCheckpoint(SafeBoundary);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		FGolemCampaignCheckpoint Loaded;
		Result = GetGameInstance()->GetSubsystem<UGolemCheckpointSubsystem>()->LoadCheckpoint(
			Campaign->CampaignId, Campaign->ScenarioKind, Loaded);
		if (Result.IsSuccess() && !ChoiceId.IsNone())
		{
			const FGolemMapRegion* Region = WorldDefinition->FindRegion(ChoiceId);
			if (Region == nullptr)
			{
				return FGolemResult::Failure(EGolemError::NotFound, ChoiceId, FText::GetEmpty());
			}
			// Explicit authored region mapping upgrades a legacy entry without editing the disk input.
			Loaded.MapPackage = Region->Map.ToSoftObjectPath().GetLongPackageName();
		}
		if (Result.IsSuccess())
		{
			Result = GetGameInstance()->GetSubsystem<UGolemCheckpointSubsystem>()->StageCheckpoint(Loaded);
		}
		if (Result.IsSuccess())
		{
			bTravelling = GetWorld()->ServerTravel(Loaded.MapPackage);
			Result = bTravelling ? FGolemResult::Success()
				: FGolemResult::Failure(EGolemError::LoadFailed, TEXT("ContinueTravel"), FText::GetEmpty());
		}
		return Result;
	}
	if (Action == TEXT("Activate"))
	{
		Result = Story->Quest->ActivateQuest(AnchorId);
		if (Result.IsSuccess())
		{
			const FGolemInventoryView Bag = GetRPGState()->GetInventory()->GetView();
			TMap<FPrimaryAssetId, int32> Counts;
			for (const FGolemItemInstance& Item : Bag.Items)
			{
				if (Item.LocationId == GolemGear::InventoryLocation)
				{
					Counts.FindOrAdd(Item.DefinitionId) += Item.Count;
				}
			}
			for (const TPair<FPrimaryAssetId, int32>& Count : Counts)
			{
				FGolemStoryEvent Fact;
				Fact.CampaignId = Campaign->CampaignId;
				Fact.RunId = RunId;
				Fact.EventName = TEXT("InventoryState");
				Fact.SubjectId = FName(*Count.Key.ToString());
				Fact.Quantity = Count.Value;
				Story->Quest->ApplyCurrentFact(Fact);
			}
		}
		return Result;
	}
	return FGolemResult::Failure(EGolemError::InvalidId, Action, FText::GetEmpty());
}

FGolemResult AGolemStoryGameMode::CaptureCheckpoint(FGolemCampaignCheckpoint& OutCheckpoint) const
{
	if (Story == nullptr || !InitializationResult.IsSuccess() || bTravelling || bStoryMutation
		|| Story->Dialogue->GetSnapshot().bActive || Story->WorldState->GetSnapshot().bSceneActive
		|| Story->WorldState->HasReservations() || GetArenaState()->GetSnapshot().AliveEnemies != 0)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("UnsafeCheckpoint"),
			NSLOCTEXT("GolemStory", "UnsafeCheckpoint", "Finish the conversation, scene and active encounter before saving."));
	}
	FGolemCampaignCheckpoint Candidate;
	FGolemResult Result = GetRPGState()->GetRPG()->CaptureSnapshot(Candidate.RPG);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Candidate.Quest = Story->Quest->GetSnapshot();
	Candidate.World = Story->WorldState->GetSnapshot();
	Candidate.Dialogue = Story->Dialogue->GetSnapshot();
	Candidate.CheckpointId = FGuid::NewGuid();
	Candidate.MapPackage = GetWorld()->GetOutermost()->GetName();
	if (GetWorld()->WorldType == EWorldType::PIE)
	{
		Candidate.MapPackage = UWorld::RemovePIEPrefix(Candidate.MapPackage);
	}
	for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		Candidate.Poise.Add(Entry.GolemId, Entry.Golem->GetCombatComponent()->GetSnapshot().Poise);
	}
	Result = Candidate.ValidateScope();
	if (Result.IsSuccess())
	{
		OutCheckpoint = MoveTemp(Candidate);
	}
	return Result;
}

FGolemResult AGolemStoryGameMode::SaveStoryCheckpoint()
{
	FGolemCampaignCheckpoint Snapshot;
	FGolemResult Result = CaptureCheckpoint(Snapshot);
	if (Result.IsSuccess())
	{
		Result = GetGameInstance()->GetSubsystem<UGolemCheckpointSubsystem>()->SaveCheckpoint(Snapshot);
	}
	UE_LOG(LogGolemStory, Display, TEXT("DurableCheckpoint result=%s id=%s"),
		Result.IsSuccess() ? TEXT("Saved") : TEXT("Rejected"), *Snapshot.CheckpointId.ToString());
	return Result;
}

FGolemResult AGolemStoryGameMode::TravelStory(APlayerController* Player, const FString& MapPackage)
{
	if (!Player->IsLocalController() || MapPackage.IsEmpty())
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("HostTravel"), FText::GetEmpty());
	}
	FGolemCampaignCheckpoint Snapshot;
	FGolemResult Result = CaptureCheckpoint(Snapshot);
	if (Result.IsSuccess())
	{
		Snapshot.MapPackage = MapPackage;
		UGolemCheckpointSubsystem* Save = GetGameInstance()->GetSubsystem<UGolemCheckpointSubsystem>();
		Result = Save->SaveCheckpoint(Snapshot);
		if (Result.IsSuccess())
		{
			Result = Save->StageCheckpoint(Snapshot);
		}
	}
	if (Result.IsSuccess())
	{
		bTravelling = GetWorld()->ServerTravel(MapPackage);
		Result = bTravelling ? FGolemResult::Success()
			: FGolemResult::Failure(EGolemError::LoadFailed, TEXT("StoryTravel"), FText::GetEmpty());
	}
	return Result;
}

void AGolemStoryGameMode::HandleArenaChanged(const FGolemPartyArenaSnapshot& Snapshot)
{
	if (Story != nullptr && Story->WorldState->GetSnapshot().EncounterPhase == EGolemStoryEncounterPhase::Active
		&& (Snapshot.Stage == EGolemArenaStage::Victory || Snapshot.Stage == EGolemArenaStage::Defeat))
	{
		const bool bSuccess = Snapshot.Stage == EGolemArenaStage::Victory
			&& (Caravan == nullptr || Caravan->GetLifeComponent()->IsAlive());
		const FGolemResult Result = Story->WorldState->FinishEncounter(bSuccess,
			Story->WorldState->GetSnapshot().EncounterRunId);
		if (!Result.IsSuccess())
		{
			UE_LOG(LogGolemStory, Error, TEXT("Encounter outcome failed: %s"), *Result.Context.ToString());
		}
	}
}

FGolemResult AGolemStoryGameMode::TryRestartDrill(APlayerController* Player)
{
	const FGolemResult Result = Super::TryRestartDrill(Player);
	if (Result.IsSuccess() && Caravan != nullptr)
	{
		Caravan->ResetCaravan();
	}
	return Result;
}

void AGolemStoryGameMode::HandleCaravanLifeChanged(const EGolemLifeState Previous, const EGolemLifeState Current)
{
	if (Story != nullptr && Current != EGolemLifeState::Alive
		&& Story->WorldState->GetSnapshot().EncounterPhase == EGolemStoryEncounterPhase::Active)
	{
		Story->WorldState->FinishEncounter(false, Story->WorldState->GetSnapshot().EncounterRunId);
	}
}

void AGolemStoryGameMode::HandleWorldChanged()
{
	const bool bActive = Story != nullptr && Story->WorldState->GetSnapshot().bSceneActive;
	FGameplayTagContainer SceneAbilityTags;
	SceneAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability")));
	if (bActive && SceneBlockedASCs.IsEmpty())
	{
		for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
		{
			UGolemAbilitySystemComponent* ASC = Entry.Golem->GetGolemAbilitySystem();
			ASC->BlockAbilitiesWithTags(SceneAbilityTags);
			SceneBlockedASCs.Add(ASC);
		}
	}
	else if (!bActive && !SceneBlockedASCs.IsEmpty())
	{
		for (const TWeakObjectPtr<UGolemAbilitySystemComponent>& ASC : SceneBlockedASCs)
		{
			if (ASC.IsValid())
			{
				ASC->UnBlockAbilitiesWithTags(SceneAbilityTags);
			}
		}
		SceneBlockedASCs.Reset();
	}
}

void AGolemStoryGameMode::HandleItemFact(const FGolemItemEvent& Event)
{
	if (Story != nullptr && Event.DemoRunId == RunId && Event.Quantity > 0)
	{
		Story->WorldState->CommitFact(Event.EventName, FName(*Event.DefinitionId.ToString()),
			NAME_None, true, Event.ConfirmationId, Event.Quantity);
	}
}

void AGolemStoryGameMode::HandleSkillFact(const FGolemSkillLabEvent& Event)
{
	if (Story != nullptr && Event.EncounterId.IsValid()
		&& Story->WorldState->GetSnapshot().EncounterPhase == EGolemStoryEncounterPhase::Active
		&& Event.EncounterId == Story->WorldState->GetSnapshot().EncounterRunId)
	{
		Story->WorldState->CommitFact(Event.EventName, Event.SubjectId, Event.ValueId, false, Event.ConfirmationId);
	}
}

AGolemStoryState* AGolemStoryGameMode::GetStoryState() const
{
	return Story;
}

const FGolemResult& AGolemStoryGameMode::GetStoryInitializationResult() const
{
	return InitializationResult;
}

void AGolemStoryGameMode::Logout(AController* Exiting)
{
	if (Story != nullptr)
	{
		Story->WorldState->ReleasePawnReservations(Exiting->GetPawn());
		Story->Dialogue->CancelConversation();
		if (Story->WorldState->GetSnapshot().bSceneActive)
		{
			Story->WorldState->FinishScene(TEXT("Disconnected"), Story->WorldState->GetSnapshot().SceneRunId);
		}
	}
	Super::Logout(Exiting);
}

void AGolemStoryGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Story != nullptr)
	{
		Story->Quest->OnSnapshotChanged.Remove(QuestHandle);
		Story->WorldState->OnSnapshotChanged.Remove(WorldHandle);
		if (Story->WorldState->GetSnapshot().bSceneActive)
		{
			Story->WorldState->FinishScene(TEXT("Travelled"), Story->WorldState->GetSnapshot().SceneRunId);
			HandleWorldChanged();
		}
	}
	if (Caravan != nullptr)
	{
		Caravan->GetLifeComponent()->OnLifeStateChanged.Remove(CaravanHandle);
	}
	if (GetArenaState() != nullptr)
	{
		GetArenaState()->OnSnapshotChanged.Remove(ArenaHandle);
	}
	if (ItemSubscription.Bus.IsValid())
	{
		ItemSubscription.Bus->Unsubscribe(ItemSubscription);
	}
	if (SkillSubscription.Bus.IsValid())
	{
		SkillSubscription.Bus->Unsubscribe(SkillSubscription);
	}
	Super::EndPlay(Reason);
}
