#include "Story/KnightStoryGameMode.h"

#include "Assets/KnightAssetManager.h"
#include "Attributes/KnightAttributeSet.h"
#include "Character/KnightCharacter.h"
#include "Checkpoint/KnightCheckpointSubsystem.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Controllers/KnightEnemyController.h"
#include "Context/KnightAIContextComponent.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "Threat/KnightThreatComponent.h"
#include "Story/KnightProtectedCaravan.h"
#include "Catalog/KnightGearCatalog.h"
#include "Definitions/KnightConversationDefinition.h"
#include "Definitions/KnightItemDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/KnightItemEvent.h"
#include "Events/KnightSkillLabEvent.h"
#include "Interaction/KnightStoryAnchor.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "Misc/ScopeExit.h"
#include "Misc/SecureHash.h"
#include "Party/KnightPartyState.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "Story/KnightStoryPlayerController.h"
#include "Story/KnightStoryHUD.h"
#include "Story/KnightStoryProfile.h"
#include "Story/KnightStoryEntry.h"
#include "Story/KnightStoryState.h"
#include "SkillLab/KnightRunestone.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "Tags/KnightGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightStory, Log, All);

namespace
{
	template <typename TDefinition>
	FKnightResult ResolveStoryDefinition(const FPrimaryAssetId& Id, TDefinition*& OutDefinition)
	{
		UKnightDefinition* Resolved = nullptr;
		UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
		const FKnightResult Result = Manager != nullptr
			? Manager->ResolveDefinition(Id, TDefinition::StaticClass(), Resolved)
			: FKnightResult::Failure(EKnightError::LoadFailed, TEXT("StoryCatalog"), FText::GetEmpty());
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

AKnightStoryGameMode::AKnightStoryGameMode()
{
	GameStateClass = AKnightStoryState::StaticClass();
	PlayerControllerClass = AKnightStoryPlayerController::StaticClass();
	HUDClass = AKnightStoryHUD::StaticClass();
	InitializationResult = FKnightResult::Failure(EKnightError::InvalidState, TEXT("StoryPending"), FText::GetEmpty());
}

FKnightResult AKnightStoryGameMode::ResolveProfile()
{
	UKnightStoryProfile* ResolvedProfile = nullptr;
	FKnightResult Result = ResolveStoryDefinition(ProfileId, ResolvedProfile);
	Profile = ResolvedProfile;
#if UE_BUILD_SHIPPING
	if (Result.IsSuccess() && Profile->bDeveloperFixture)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("DeveloperScenario"), FText::GetEmpty());
	}
#endif
	if (Result.IsSuccess())
	{
		UKnightCampaignDefinition* ResolvedCampaign = nullptr;
		Result = ResolveStoryDefinition(Profile->CampaignDefinitionId, ResolvedCampaign);
		Campaign = ResolvedCampaign;
	}
	if (Result.IsSuccess())
	{
		Result = Campaign->ValidateCampaign();
	}
	if (Result.IsSuccess())
	{
		UKnightStoryWorldDefinition* ResolvedWorld = nullptr;
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
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("ProductionSeed"), FText::GetEmpty());
	}
	TSet<FName> RewardQuests;
	for (const FKnightStoryReward& Reward : Profile->Rewards)
	{
		const FKnightQuestDefinition* Quest = Campaign->FindQuest(Reward.QuestId);
		if (Quest == nullptr || Quest->RewardKey != Reward.RewardKey || RewardQuests.Contains(Reward.QuestId)
			|| Reward.Items.IsEmpty() || Reward.Currency < 0)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, Reward.QuestId, FText::GetEmpty());
		}
		RewardQuests.Add(Reward.QuestId);
		for (const FKnightGearRewardItem& Item : Reward.Items)
		{
			UKnightItemDefinition* ItemDefinition = nullptr;
			Result = ResolveStoryDefinition(Item.DefinitionId, ItemDefinition);
			if (!Result.IsSuccess() || !ItemDefinition->ValidateItem().IsSuccess() || Item.Count < 1 || Item.Count > 10000)
			{
				return FKnightResult::Failure(EKnightError::InvalidData, FName(*Item.DefinitionId.ToString()), FText::GetEmpty());
			}
		}
	}
	for (const FKnightStoryInteraction& Interaction : WorldDefinition->Interactions)
	{
		if (!Interaction.RequiredQuestId.IsNone())
		{
			const FKnightQuestDefinition* Quest = Campaign->FindQuest(Interaction.RequiredQuestId);
			if (Quest == nullptr || !Interaction.RequiredObjectiveId.IsNone()
				&& !Quest->Objectives.ContainsByPredicate([&Interaction](const FKnightObjectiveDefinition& Objective)
				{
					return Objective.ObjectiveId == Interaction.RequiredObjectiveId;
				}))
			{
				return FKnightResult::Failure(EKnightError::NotFound, Interaction.RequiredQuestId, FText::GetEmpty());
			}
		}
		if (Interaction.Action == EKnightStoryAction::Conversation)
		{
			UKnightConversationDefinition* Conversation = nullptr;
			Result = ResolveStoryDefinition(Interaction.ConversationId, Conversation);
			if (!Result.IsSuccess() || !Conversation->ValidateConversation().IsSuccess())
			{
				return FKnightResult::Failure(EKnightError::NotFound,
					FName(*Interaction.ConversationId.ToString()), FText::GetEmpty());
			}
		}
		if (Interaction.Action == EKnightStoryAction::Reward && !RewardQuests.Contains(Interaction.RewardQuestId))
		{
			return FKnightResult::Failure(EKnightError::NotFound, Interaction.RewardQuestId, FText::GetEmpty());
		}
	}
	return Result;
}

void AKnightStoryGameMode::InitGameState()
{
	for (TActorIterator<AKnightStoryEntry> It(GetWorld()); It; ++It)
	{
		ProfileId = It->ProfileId;
		break;
	}
	InitializationResult = ResolveProfile();
	UKnightCheckpointSubsystem* Save = GetGameInstance()->GetSubsystem<UKnightCheckpointSubsystem>();
	FString CheckpointFixture;
#if !UE_BUILD_SHIPPING
	FParse::Value(FCommandLine::Get(), TEXT("KnightCheckpointFixture="), CheckpointFixture);
#endif
	if (InitializationResult.IsSuccess()
		&& (FParse::Param(FCommandLine::Get(), TEXT("KnightContinue")) || !CheckpointFixture.IsEmpty()))
	{
		FKnightCampaignCheckpoint Loaded;
		InitializationResult = CheckpointFixture.IsEmpty()
			? Save->LoadCheckpoint(Campaign->CampaignId, Campaign->ScenarioKind, Loaded)
			: UKnightCheckpointSubsystem::ReadCheckpointFile(CheckpointFixture, Loaded);
		if (InitializationResult.IsSuccess())
		{
			InitializationResult = Save->StageCheckpoint(Loaded);
		}
	}
	const FKnightCampaignCheckpoint* Staged = Save->GetStagedCheckpoint();
	if (InitializationResult.IsSuccess() && Staged != nullptr
		&& (Staged->Quest.CampaignId != Campaign->CampaignId || Staged->Quest.ScenarioKind != Campaign->ScenarioKind))
	{
		InitializationResult = FKnightResult::Failure(EKnightError::Conflict, TEXT("ScenarioCheckpoint"), FText::GetEmpty());
	}
	RunId = Staged != nullptr ? Staged->Quest.RunId : FGuid::NewGuid();
	Super::InitGameState();
	Story = GetGameState<AKnightStoryState>();
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
			for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
			{
				InitializationResult = Entry.Knight->GetCombatComponent()->RestoreSnapshotPoise(
					Staged->Poise.FindChecked(Entry.KnightId));
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

FKnightEntityId AKnightStoryGameMode::CreateKnightIdentity(const int32 Slot, const FPrimaryAssetId& HeroId) const
{
	const FKnightCampaignCheckpoint* Staged =
		GetGameInstance()->GetSubsystem<UKnightCheckpointSubsystem>()->GetStagedCheckpoint();
	if (Staged != nullptr)
	{
		const FKnightRPGKnightView* Knight = Staged->RPG.View.Knights.FindByPredicate(
			[HeroId](const FKnightRPGKnightView& Row)
			{
				return Row.HeroDefinitionId == HeroId;
			});
		return Knight != nullptr ? Knight->KnightId : FKnightEntityId();
	}
	return Super::CreateKnightIdentity(Slot, HeroId);
}

FKnightResult AKnightStoryGameMode::InitializeRPGDomains(UKnightInventoryComponent* Inventory,
	UKnightRPGComponent* RPG, TConstArrayView<FKnightRPGKnightRegistration> Registrations)
{
	if (!InitializationResult.IsSuccess())
	{
		return InitializationResult;
	}
	const FKnightCampaignCheckpoint* Staged =
		GetGameInstance()->GetSubsystem<UKnightCheckpointSubsystem>()->GetStagedCheckpoint();
	return Staged != nullptr ? RPG->TryRestoreSnapshot(Inventory, Staged->RPG, Registrations)
		: Super::InitializeRPGDomains(Inventory, RPG, Registrations);
}

FKnightResult AKnightStoryGameMode::ConfigureRPGDomains(UKnightInventoryComponent* Inventory, UKnightRPGComponent* RPG)
{
	if (Profile->bDeveloperFixture)
	{
		FKnightInventoryRequest Request;
		Request.RequestId = FGuid::NewGuid();
		Request.ExpectedRevision = Inventory->GetView().Revision;
		for (const FKnightGearRewardItem& Seed : Profile->DeveloperSeedItems)
		{
			UKnightItemDefinition* Definition = nullptr;
			FKnightResult Resolved = ResolveStoryDefinition(Seed.DefinitionId, Definition);
			if (!Resolved.IsSuccess() || Seed.Count < 1 || Seed.Count > 10000)
			{
				return FKnightResult::Failure(EKnightError::InvalidData, TEXT("FixtureSeed"), FText::GetEmpty());
			}
			int32 Remaining = Seed.Count;
			while (Remaining > 0)
			{
				FKnightInventoryMutation& Mutation = Request.Mutations.AddDefaulted_GetRef();
				Mutation.Item.InstanceId = FGuid::NewGuid();
				Mutation.Item.DefinitionId = Seed.DefinitionId;
				Mutation.Item.Count = FMath::Min(Remaining, Definition->GetMaximumStack());
				Remaining -= Mutation.Item.Count;
			}
		}
		const FKnightResult Seed = Inventory->TryApplyTransaction(Request).Result;
		if (!Seed.IsSuccess())
		{
			return Seed;
		}
	}
	// Demo profile starts at level one, zero currency and an empty bag. No CatalogLab unlock/starter seed.
	if (Profile->bDeveloperFixture && !Profile->DeveloperBuilds.IsEmpty())
	{
		const FKnightResult Builds = RPG->ConfigureSkillLabBuilds(Profile->DeveloperBuilds);
		if (!Builds.IsSuccess())
		{
			return Builds;
		}
	}
	return RPG->ConfigureGearDemo(RunId, 0, Profile->bDeveloperFixture ? Profile->DeveloperLevel : 1,
		Profile->bDeveloperFixture && Profile->bDeveloperStarterEquipment);
}

bool AKnightStoryGameMode::ShouldIssueLegacyEncounterRewards() const
{
	return false;
}

bool AKnightStoryGameMode::ShouldAdvanceWaveToBoss() const
{
	return false;
}

FKnightResult AKnightStoryGameMode::BuildCaller(APlayerController* Player, FKnightRPGCallerContext& OutCaller) const
{
	FKnightResult Result = Super::BuildCaller(Player, OutCaller);
	if (Result.IsSuccess() && InitializationResult.IsSuccess() && Story != nullptr)
	{
		// School is the authored non-combat sanctuary; no lab reward/level bypass is involved.
		OutCaller.bInSafeZone = IsInUpgradeSafeZone(OutCaller.Pawn.Get());
		OutCaller.EncounterId = Story->WorldState->GetSnapshot().EncounterRunId;
	}
	return Result;
}

bool AKnightStoryGameMode::IsInUpgradeSafeZone(const AActor* Knight) const
{
	const UKnightLifeComponent* Life = IsValid(Knight) ? Knight->FindComponentByClass<UKnightLifeComponent>() : nullptr;
	return InitializationResult.IsSuccess() && Story != nullptr && Life != nullptr && Life->IsAlive()
		&& Knight->GetWorld() == GetWorld() && GetArenaState()->GetSnapshot().AliveEnemies == 0
		&& Story->WorldState->GetSnapshot().EncounterPhase != EKnightStoryEncounterPhase::Active
		&& !Story->WorldState->GetSnapshot().bSceneActive;
}

void AKnightStoryGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (!InitializationResult.IsSuccess() || !GetRPGInitializationResult().IsSuccess())
	{
		UE_LOG(LogKnightStory, Error, TEXT("Story composition unavailable: %s / %s"),
			*InitializationResult.Context.ToString(), *GetRPGInitializationResult().Context.ToString());
		return;
	}
	for (TActorIterator<AKnightStoryAnchor> It(GetWorld()); It; ++It)
	{
		InitializationResult = Story->WorldState->BindAnchor(*It);
		if (!InitializationResult.IsSuccess())
		{
			UE_LOG(LogKnightStory, Error, TEXT("Duplicate/missing story anchor %s"), *It->AnchorId.ToString());
			return;
		}
	}
	QuestHandle = Story->Quest->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleQuestChanged);
	ArenaHandle = GetArenaState()->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleArenaChanged);
	WorldHandle = Story->WorldState->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleWorldChanged);
	for (TActorIterator<AKnightProtectedCaravan> It(GetWorld()); It; ++It)
	{
		Caravan = *It;
		CaravanHandle = Caravan->GetLifeComponent()->OnLifeStateChanged.AddUObject(this, &ThisClass::HandleCaravanLifeChanged);
	}
	UKnightEventSubsystem* Bus = GetWorld()->GetSubsystem<UKnightEventSubsystem>();
	ItemSubscription = Bus->Subscribe<FKnightItemEvent>(this, [this](const FKnightItemEvent& Event)
	{
		HandleItemFact(Event);
	});
	SkillSubscription = Bus->Subscribe<FKnightSkillLabEvent>(this, [this](const FKnightSkillLabEvent& Event)
	{
		HandleSkillFact(Event);
	});
	HandleQuestChanged();
	UE_LOG(LogKnightStory, Display, TEXT("StoryReady campaign=%s run=%s content=%d canonical=3"),
		*Campaign->CampaignId.ToString(), *RunId.ToString(), Campaign->ContentRevision);
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("KnightStoryColdSmoke")))
	{
		const bool bPending = FParse::Param(FCommandLine::Get(), TEXT("KnightStoryPendingSmoke"));
		const FKnightQuestProgress* Append = Story->Quest->FindProgress(TEXT("Demo_Append"));
		bool bPassed = Campaign->ContentRevision == 2
			&& Story->Quest->FindProgress(TEXT("ActI_Q1")) != nullptr
			&& Story->Quest->FindProgress(TEXT("ActI_Q1"))->Phase == EKnightQuestPhase::Complete
			&& Story->Quest->FindProgress(TEXT("ActI_Q2")) != nullptr
			&& Story->Quest->FindProgress(TEXT("ActI_Q2"))->Phase ==
				(bPending ? EKnightQuestPhase::RewardPending : EKnightQuestPhase::Complete)
			&& GetRPGState()->GetInventory()->GetView().Items.Num() == (bPending ? 0 : 3)
			&& GetRPGState()->GetRPG()->GetView().ClaimedRewards.Num() == (bPending ? 0 : 1)
			&& Append != nullptr && Append->Phase == (bPending ? EKnightQuestPhase::Locked : EKnightQuestPhase::Available);
		if (bPassed && FParse::Param(FCommandLine::Get(), TEXT("KnightStoryColdResave")))
		{
			bPassed = SaveStoryCheckpoint().IsSuccess();
		}
		const FString Evidence = FString::Printf(TEXT("result=%s campaign=%s run=%s content=%d receipts=%d items=%d appendedAvailable=%d pendingExpected=%d\n"),
			bPassed ? TEXT("PASS") : TEXT("FAIL"), *Campaign->CampaignId.ToString(), *RunId.ToString(),
			Campaign->ContentRevision, GetRPGState()->GetRPG()->GetView().ClaimedRewards.Num(),
			GetRPGState()->GetInventory()->GetView().Items.Num(), Append != nullptr && Append->Phase == EKnightQuestPhase::Available, bPending);
		UE_LOG(LogKnightStory, Display, TEXT("ColdProcessProof %s"), *Evidence);
		FFileHelper::SaveStringToFile(Evidence, *(FPaths::ProjectSavedDir() / TEXT("M6ColdProcessProof.txt")));
		FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
	}
#endif
}

void AKnightStoryGameMode::HandleQuestChanged()
{
	if (Story == nullptr || bStoryMutation || GetRPGState()->GetRPG()->IsTransactionInProgress())
	{
		return;
	}
	for (const FKnightStoryReward& Reward : Profile->Rewards)
	{
		const FKnightQuestProgress* Progress = Story->Quest->FindProgress(Reward.QuestId);
		if (Progress == nullptr || Progress->Phase != EKnightQuestPhase::RewardPending)
		{
			continue;
		}
		FKnightGearRewardBundle Bundle;
		Bundle.RewardId = StoryReceipt(RunId, Reward.RewardKey);
		Bundle.DemoRunId = RunId;
		Bundle.EncounterId = Story->WorldState->GetSnapshot().EncounterRunId;
		Bundle.Currency = Reward.Currency;
		Bundle.Level = 1;
		Bundle.Items = Reward.Items;
		const TArray<FKnightGearRewardBundle> ExistingOffers = GetRPGState()->GetRPG()->GetRewardOffers();
		if (ExistingOffers.ContainsByPredicate([&Bundle](const FKnightGearRewardBundle& Existing)
		{
			return Existing.RewardId == Bundle.RewardId;
		}))
		{
			continue;
		}
		if (Reward.bReuseExistingHammers)
		{
			Bundle.Items.Reset();
			const FKnightRPGView PartyView = GetRPGState()->GetRPG()->GetView();
			for (int32 Index = 0; Index < PartyView.Knights.Num(); ++Index)
			{
				const FKnightRPGKnightView& Knight = PartyView.Knights[Index];
				const FKnightEquipmentAssignment* Weapon = Knight.FindAssignment(KnightGameplayTags::Slot_Equipment_Weapon);
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
		const FKnightResult Registered = GetRPGState()->GetRPG()->RegisterRewardBundle(Bundle);
		if (!Registered.IsSuccess())
		{
			UE_LOG(LogKnightStory, Error, TEXT("Story reward offer failed: %s"), *Registered.Context.ToString());
		}
	}
}

FKnightResult AKnightStoryGameMode::ClaimQuestReward(APlayerController* Player, const FName QuestId)
{
	const FKnightQuestProgress* Progress = Story->Quest->FindProgress(QuestId);
	const FKnightStoryReward* Reward = Profile->Rewards.FindByPredicate([QuestId](const FKnightStoryReward& Row)
	{
		return Row.QuestId == QuestId;
	});
	if (Progress == nullptr || Reward == nullptr || Progress->Phase != EKnightQuestPhase::RewardPending)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, QuestId,
			NSLOCTEXT("KnightStory", "RewardUnavailable", "This quest has no pending reward."));
	}
	HandleQuestChanged();
	FKnightRPGCallerContext Caller;
	FKnightResult Result = BuildCaller(Player, Caller);
	if (!Result.IsSuccess() || !Player->IsLocalController())
	{
		return Result.IsSuccess() ? FKnightResult::Failure(EKnightError::NotOwned,
			QuestId, FText::GetEmpty()) : Result;
	}
	const FGuid Receipt = StoryReceipt(RunId, Reward->RewardKey);
	if (GetRPGState()->GetRPG()->GetView().ClaimedRewards.Contains(Receipt))
	{
		return Story->Quest->CommitReward(QuestId, Receipt);
	}
	FKnightRPGCommand Command;
	Command.Operation = EKnightRPGOperation::ClaimReward;
	Command.RequestId = Receipt;
	Command.ExpectedKnightId = Caller.KnightId;
	Command.InventoryId = GetRPGState()->GetInventory()->GetView().InventoryId;
	Command.ExpectedInventoryRevision = GetRPGState()->GetInventory()->GetView().Revision;
	Command.ExpectedKnightRevision = GetRPGState()->GetRPG()->GetKnightView(Caller.KnightId).Revision;
	Command.RewardId = Receipt;
	Result = GetRPGState()->GetRPG()->ExecuteCommand(Caller, Command).Result;
	return Result.IsSuccess() ? Story->Quest->CommitReward(QuestId, Receipt) : Result;
}

FKnightResult AKnightStoryGameMode::InteractAnchor(APlayerController* Player, const FName AnchorId,
	const FGuid& RequestId)
{
	return InteractAnchor(Player, AnchorId, RequestId, NAME_None);
}

FKnightResult AKnightStoryGameMode::InteractAnchor(APlayerController* Player, const FName AnchorId,
	const FGuid& RequestId, const FName ChoiceId)
{
	const FKnightStoryInteraction* Authored = WorldDefinition->FindInteraction(AnchorId);
	if (Authored == nullptr)
	{
		return FKnightResult::Failure(EKnightError::NotFound, AnchorId, FText::GetEmpty());
	}
	FKnightStoryInteraction Resolved = *Authored;
	if (!ChoiceId.IsNone())
	{
		if (!Resolved.AllowedChoices.Contains(ChoiceId))
		{
			return FKnightResult::Failure(EKnightError::InvalidId, ChoiceId, FText::GetEmpty());
		}
		Resolved.ValueId = ChoiceId;
	}
	const FKnightStoryInteraction* Interaction = &Resolved;
	const FName* BranchChoice = Story->Quest->GetSnapshot().Branches.Find(Interaction->RequiredBranchId);
	if (!Interaction->RequiredBranchId.IsNone()
		&& (BranchChoice == nullptr || *BranchChoice != Interaction->RequiredBranchChoice))
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("BranchGate"),
			NSLOCTEXT("KnightStory", "BranchGate", "This route is closed by the retained campaign choice."));
	}
	const FKnightQuestProgress* Progress = Interaction->RequiredQuestId.IsNone()
		? nullptr : Story->Quest->FindProgress(Interaction->RequiredQuestId);
	if (!Interaction->RequiredQuestId.IsNone() && (Progress == nullptr || Progress->Phase != EKnightQuestPhase::Active))
	{
		return FKnightResult::Failure(EKnightError::InvalidState, Interaction->RequiredQuestId,
			NSLOCTEXT("KnightStory", "QuestLocked", "Follow the current lesson before using this station."));
	}
	if (!Interaction->RequiredObjectiveId.IsNone())
	{
		const FKnightQuestDefinition* Definition = Campaign->FindQuest(Progress->QuestId);
		FName NextObjective;
		for (const FKnightObjectiveDefinition& Objective : Definition->Objectives)
		{
			if (Progress->Counts.FindChecked(Objective.ObjectiveId) < Objective.RequiredCount)
			{
				NextObjective = Objective.ObjectiveId;
				break;
			}
		}
		if (NextObjective != Interaction->RequiredObjectiveId)
		{
			return FKnightResult::Failure(EKnightError::InvalidState, Interaction->RequiredObjectiveId,
				NSLOCTEXT("KnightStory", "ForgeOrder", "Use the forge stations in the displayed order."));
		}
	}
	FGuid Token;
	FKnightResult Result = Story->WorldState->ReserveInteraction(AnchorId, Player->GetPawn(), RequestId,
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
	case EKnightStoryAction::Branch:
		if (!Player->IsLocalController())
		{
			return FKnightResult::Failure(EKnightError::NotOwned, TEXT("HostBranch"), FText::GetEmpty());
		}
		Result = Story->Quest->CommitBranch(Interaction->SubjectId, Interaction->ValueId,
			Story->Quest->GetSnapshot().Revision);
		return Result.IsSuccess() ? Story->WorldState->CommitFact(TEXT("BranchCommitted"),
			Interaction->SubjectId, Interaction->ValueId, true, RequestId) : Result;
	case EKnightStoryAction::Conversation:
	{
		if (!Player->IsLocalController())
		{
			return FKnightResult::Failure(EKnightError::NotOwned, TEXT("ConversationHost"), FText::GetEmpty());
		}
		UKnightConversationDefinition* Conversation = nullptr;
		Result = ResolveStoryDefinition(Interaction->ConversationId, Conversation);
		return Result.IsSuccess() ? Story->Dialogue->StartConversation(Conversation) : Result;
	}
	case EKnightStoryAction::Reward:
		Story->WorldState->ReleaseReservation(Token);
		return ClaimQuestReward(Player, Interaction->RewardQuestId);
	case EKnightStoryAction::Checkpoint:
		Story->WorldState->ReleaseReservation(Token);
		if (Player->IsLocalController())
		{
			Story->WorldState->SetCheckpoint(AnchorId);
		}
		return Player->IsLocalController() ? SaveStoryCheckpoint()
			: FKnightResult::Failure(EKnightError::NotOwned, TEXT("HostCheckpoint"), FText::GetEmpty());
	case EKnightStoryAction::Travel:
		Story->WorldState->ReleaseReservation(Token);
		return TravelStory(Player, Interaction->Destination.ToSoftObjectPath().GetLongPackageName());
	case EKnightStoryAction::Encounter:
		Result = Story->WorldState->BeginEncounter(Interaction->SubjectId);
		if (Result.IsSuccess())
		{
			Story->Quest->SetEncounterRun(Story->WorldState->GetSnapshot().EncounterRunId);
			Result = StartFixtureEncounter(EKnightArenaStage::Wave);
			if (Result.IsSuccess())
			{
				const FGuid EncounterRun = Story->WorldState->GetSnapshot().EncounterRunId;
				for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
				{
					if (UKnightSkillRuntimeComponent* Runtime = Entry.Knight->FindComponentByClass<UKnightSkillRuntimeComponent>())
					{
						Runtime->SetEncounterId(EncounterRun);
					}
				}
				for (TActorIterator<AKnightRunestone> It(GetWorld()); It; ++It)
				{
					It->ResetFixture(EncounterRun);
				}
			}
			if (Result.IsSuccess() && Caravan != nullptr)
			{
				for (AKnightArenaEnemy* Enemy : GetEncounterEnemies())
				{
					Enemy->GetThreatComponent()->AddThreat(Caravan, 10000.0f);
					if (AKnightEnemyController* Controller = Cast<AKnightEnemyController>(Enemy->GetController()))
					{
						Controller->GetAIContext()->SetPerceivedActor(Caravan, true);
					}
				}
			}
		}
		return Result;
	case EKnightStoryAction::Scene:
	{
		Story->WorldState->ReleaseReservation(Token);
		FKnightCampaignCheckpoint Boundary;
		Result = CaptureCheckpoint(Boundary);
		return Result.IsSuccess() && Player->IsLocalController() ? Story->WorldState->BeginScene(Interaction->SubjectId)
			: Result.IsSuccess() ? FKnightResult::Failure(EKnightError::NotOwned, TEXT("HostScene"), FText::GetEmpty()) : Result;
	}
	case EKnightStoryAction::Deliver:
	{
		if (!Player->IsLocalController())
		{
			return FKnightResult::Failure(EKnightError::NotOwned, TEXT("HostDelivery"),
				NSLOCTEXT("KnightStory", "HostDelivery", "The host confirms shared quest deliveries."));
		}
		FKnightRPGCallerContext Caller;
		Result = BuildCaller(Player, Caller);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		const FKnightInventoryView Bag = GetRPGState()->GetInventory()->GetView();
		const FKnightItemInstance* Stack = Bag.Items.FindByPredicate([Interaction](const FKnightItemInstance& Item)
		{
			return Item.DefinitionId == Interaction->DeliveryItemId && Item.Count >= Interaction->DeliveryCount
				&& Item.LocationId == KnightGear::InventoryLocation;
		});
		if (Stack == nullptr)
		{
			return FKnightResult::Failure(EKnightError::InsufficientResources, TEXT("QuestDelivery"),
				NSLOCTEXT("KnightStory", "DeliveryMissing", "Bring the required item before delivering it."));
		}
		FKnightRPGCommand Command;
		Command.Operation = EKnightRPGOperation::DeliverQuestItem;
		Command.RequestId = RequestId;
		Command.ExpectedKnightId = Caller.KnightId;
		Command.InventoryId = Bag.InventoryId;
		Command.ExpectedInventoryRevision = Bag.Revision;
		Command.ExpectedKnightRevision = GetRPGState()->GetRPG()->GetKnightView(Caller.KnightId).Revision;
		Command.ItemInstanceId = Stack->InstanceId;
		Command.ExpectedItemRevision = Stack->ItemRevision;
		Command.ItemDefinitionId = Interaction->DeliveryItemId;
		Command.Quantity = Interaction->DeliveryCount;
		Caller.bQuestDeliveryAuthorized = true;
		return GetRPGState()->GetRPG()->ExecuteCommand(Caller, Command).Result;
	}
	case EKnightStoryAction::Forge:
	case EKnightStoryAction::Fact:
	{
		{
			TGuardValue<bool> MutationGuard(bStoryMutation, true);
			Result = Story->WorldState->CommitFact(Interaction->EventName, Interaction->SubjectId, Interaction->ValueId);
		}
		HandleQuestChanged();
		return Result;
	}
	}
	return FKnightResult::Failure(EKnightError::InvalidData, AnchorId, FText::GetEmpty());
}

FKnightResult AKnightStoryGameMode::TryInteract(APlayerController* Player)
{
	FKnightRPGCallerContext Caller;
	FKnightResult Result = BuildCaller(Player, Caller);
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
	AKnightStoryAnchor* Nearest = nullptr;
	double Distance = TNumericLimits<double>::Max();
	for (TActorIterator<AKnightStoryAnchor> It(GetWorld()); It; ++It)
	{
		const FKnightStoryInteraction* Interaction = WorldDefinition->FindInteraction(It->AnchorId);
		const double Candidate = FVector::DistSquared(Player->GetPawn()->GetActorLocation(), It->GetActorLocation());
		if (Interaction != nullptr && Candidate <= FMath::Square(Interaction->InteractionRange) && Candidate < Distance)
		{
			Nearest = *It;
			Distance = Candidate;
		}
	}
	return Nearest != nullptr ? InteractAnchor(Player, Nearest->AnchorId, FGuid::NewGuid())
		: FKnightResult::Failure(EKnightError::OutOfRange, TEXT("StoryStation"),
			NSLOCTEXT("KnightStory", "FindStation", "Move close to a labelled station and use Interact."));
}

FKnightResult AKnightStoryGameMode::ExecuteStoryAction(APlayerController* Player, const FName Action,
	const FName AnchorId, const FName ChoiceId, const FKnightEntityId& ExpectedKnight,
	const FGuid& RequestId, const int64 ExpectedRevision)
{
	FKnightRPGCallerContext Caller;
	FKnightResult Result = BuildCaller(Player, Caller);
	if (!Result.IsSuccess() || Caller.KnightId != ExpectedKnight || !RequestId.IsValid()
		|| bTravelling || bStoryMutation)
	{
		return Result.IsSuccess() ? FKnightResult::Failure(EKnightError::InvalidState,
			TEXT("StoryCommand"), FText::GetEmpty()) : Result;
	}
	if (Action == TEXT("Interact"))
	{
		return Story->WorldState->GetSnapshot().Revision == ExpectedRevision
			? InteractAnchor(Player, AnchorId, RequestId, ChoiceId)
			: FKnightResult::Failure(EKnightError::InvalidRevision, AnchorId, FText::GetEmpty());
	}
	if (!Player->IsLocalController())
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("HostDecision"), FText::GetEmpty());
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
		FKnightCampaignCheckpoint Boundary;
		Result = CaptureCheckpoint(Boundary);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		GetGameInstance()->GetSubsystem<UKnightCheckpointSubsystem>()->ConsumeStagedCheckpoint();
		bTravelling = GetWorld()->ServerTravel(Boundary.MapPackage);
		return bTravelling ? FKnightResult::Success()
			: FKnightResult::Failure(EKnightError::LoadFailed, TEXT("NewCampaignTravel"), FText::GetEmpty());
	}
	if (Action == TEXT("Continue"))
	{
		FKnightCampaignCheckpoint SafeBoundary;
		Result = CaptureCheckpoint(SafeBoundary);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		FKnightCampaignCheckpoint Loaded;
		Result = GetGameInstance()->GetSubsystem<UKnightCheckpointSubsystem>()->LoadCheckpoint(
			Campaign->CampaignId, Campaign->ScenarioKind, Loaded);
		if (Result.IsSuccess() && !ChoiceId.IsNone())
		{
			const FKnightMapRegion* Region = WorldDefinition->FindRegion(ChoiceId);
			if (Region == nullptr)
			{
				return FKnightResult::Failure(EKnightError::NotFound, ChoiceId, FText::GetEmpty());
			}
			// Explicit authored region mapping upgrades a legacy entry without editing the disk input.
			Loaded.MapPackage = Region->Map.ToSoftObjectPath().GetLongPackageName();
		}
		if (Result.IsSuccess())
		{
			Result = GetGameInstance()->GetSubsystem<UKnightCheckpointSubsystem>()->StageCheckpoint(Loaded);
		}
		if (Result.IsSuccess())
		{
			bTravelling = GetWorld()->ServerTravel(Loaded.MapPackage);
			Result = bTravelling ? FKnightResult::Success()
				: FKnightResult::Failure(EKnightError::LoadFailed, TEXT("ContinueTravel"), FText::GetEmpty());
		}
		return Result;
	}
	if (Action == TEXT("Activate"))
	{
		Result = Story->Quest->ActivateQuest(AnchorId);
		if (Result.IsSuccess())
		{
			const FKnightInventoryView Bag = GetRPGState()->GetInventory()->GetView();
			TMap<FPrimaryAssetId, int32> Counts;
			for (const FKnightItemInstance& Item : Bag.Items)
			{
				if (Item.LocationId == KnightGear::InventoryLocation)
				{
					Counts.FindOrAdd(Item.DefinitionId) += Item.Count;
				}
			}
			for (const TPair<FPrimaryAssetId, int32>& Count : Counts)
			{
				FKnightStoryEvent Fact;
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
	return FKnightResult::Failure(EKnightError::InvalidId, Action, FText::GetEmpty());
}

FKnightResult AKnightStoryGameMode::CaptureCheckpoint(FKnightCampaignCheckpoint& OutCheckpoint) const
{
	if (Story == nullptr || !InitializationResult.IsSuccess() || bTravelling || bStoryMutation
		|| Story->Dialogue->GetSnapshot().bActive || Story->WorldState->GetSnapshot().bSceneActive
		|| Story->WorldState->HasReservations() || GetArenaState()->GetSnapshot().AliveEnemies != 0)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("UnsafeCheckpoint"),
			NSLOCTEXT("KnightStory", "UnsafeCheckpoint", "Finish the conversation, scene and active encounter before saving."));
	}
	FKnightCampaignCheckpoint Candidate;
	FKnightResult Result = GetRPGState()->GetRPG()->CaptureSnapshot(Candidate.RPG);
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
	for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		Candidate.Poise.Add(Entry.KnightId, Entry.Knight->GetCombatComponent()->GetSnapshot().Poise);
	}
	Result = Candidate.ValidateScope();
	if (Result.IsSuccess())
	{
		OutCheckpoint = MoveTemp(Candidate);
	}
	return Result;
}

FKnightResult AKnightStoryGameMode::SaveStoryCheckpoint()
{
	FKnightCampaignCheckpoint Snapshot;
	FKnightResult Result = CaptureCheckpoint(Snapshot);
	if (Result.IsSuccess())
	{
		Result = GetGameInstance()->GetSubsystem<UKnightCheckpointSubsystem>()->SaveCheckpoint(Snapshot);
	}
	UE_LOG(LogKnightStory, Display, TEXT("DurableCheckpoint result=%s id=%s"),
		Result.IsSuccess() ? TEXT("Saved") : TEXT("Rejected"), *Snapshot.CheckpointId.ToString());
	return Result;
}

FKnightResult AKnightStoryGameMode::TravelStory(APlayerController* Player, const FString& MapPackage)
{
	if (!Player->IsLocalController() || MapPackage.IsEmpty())
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("HostTravel"), FText::GetEmpty());
	}
	FKnightCampaignCheckpoint Snapshot;
	FKnightResult Result = CaptureCheckpoint(Snapshot);
	if (Result.IsSuccess())
	{
		Snapshot.MapPackage = MapPackage;
		UKnightCheckpointSubsystem* Save = GetGameInstance()->GetSubsystem<UKnightCheckpointSubsystem>();
		Result = Save->SaveCheckpoint(Snapshot);
		if (Result.IsSuccess())
		{
			Result = Save->StageCheckpoint(Snapshot);
		}
	}
	if (Result.IsSuccess())
	{
		bTravelling = GetWorld()->ServerTravel(MapPackage);
		Result = bTravelling ? FKnightResult::Success()
			: FKnightResult::Failure(EKnightError::LoadFailed, TEXT("StoryTravel"), FText::GetEmpty());
	}
	return Result;
}

void AKnightStoryGameMode::HandleArenaChanged(const FKnightPartyArenaSnapshot& Snapshot)
{
	if (Story != nullptr && Story->WorldState->GetSnapshot().EncounterPhase == EKnightStoryEncounterPhase::Active
		&& (Snapshot.Stage == EKnightArenaStage::Victory || Snapshot.Stage == EKnightArenaStage::Defeat))
	{
		const bool bSuccess = Snapshot.Stage == EKnightArenaStage::Victory
			&& (Caravan == nullptr || Caravan->GetLifeComponent()->IsAlive());
		const FKnightResult Result = Story->WorldState->FinishEncounter(bSuccess,
			Story->WorldState->GetSnapshot().EncounterRunId);
		if (!Result.IsSuccess())
		{
			UE_LOG(LogKnightStory, Error, TEXT("Encounter outcome failed: %s"), *Result.Context.ToString());
		}
	}
}

FKnightResult AKnightStoryGameMode::TryRestartDrill(APlayerController* Player)
{
	const FKnightResult Result = Super::TryRestartDrill(Player);
	if (Result.IsSuccess() && Caravan != nullptr)
	{
		Caravan->ResetCaravan();
	}
	return Result;
}

void AKnightStoryGameMode::HandleCaravanLifeChanged(const EKnightLifeState Previous, const EKnightLifeState Current)
{
	if (Story != nullptr && Current != EKnightLifeState::Alive
		&& Story->WorldState->GetSnapshot().EncounterPhase == EKnightStoryEncounterPhase::Active)
	{
		Story->WorldState->FinishEncounter(false, Story->WorldState->GetSnapshot().EncounterRunId);
	}
}

void AKnightStoryGameMode::HandleWorldChanged()
{
	const bool bActive = Story != nullptr && Story->WorldState->GetSnapshot().bSceneActive;
	FGameplayTagContainer SceneAbilityTags;
	SceneAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability")));
	if (bActive && SceneBlockedASCs.IsEmpty())
	{
		for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
		{
			UKnightAbilitySystemComponent* ASC = Entry.Knight->GetKnightAbilitySystem();
			ASC->BlockAbilitiesWithTags(SceneAbilityTags);
			SceneBlockedASCs.Add(ASC);
		}
	}
	else if (!bActive && !SceneBlockedASCs.IsEmpty())
	{
		for (const TWeakObjectPtr<UKnightAbilitySystemComponent>& ASC : SceneBlockedASCs)
		{
			if (ASC.IsValid())
			{
				ASC->UnBlockAbilitiesWithTags(SceneAbilityTags);
			}
		}
		SceneBlockedASCs.Reset();
	}
}

void AKnightStoryGameMode::HandleItemFact(const FKnightItemEvent& Event)
{
	if (Story != nullptr && Event.DemoRunId == RunId && Event.Quantity > 0)
	{
		Story->WorldState->CommitFact(Event.EventName, FName(*Event.DefinitionId.ToString()),
			NAME_None, true, Event.ConfirmationId, Event.Quantity);
	}
}

void AKnightStoryGameMode::HandleSkillFact(const FKnightSkillLabEvent& Event)
{
	if (Story != nullptr && Event.EncounterId.IsValid()
		&& Story->WorldState->GetSnapshot().EncounterPhase == EKnightStoryEncounterPhase::Active
		&& Event.EncounterId == Story->WorldState->GetSnapshot().EncounterRunId)
	{
		Story->WorldState->CommitFact(Event.EventName, Event.SubjectId, Event.ValueId, false, Event.ConfirmationId);
	}
}

AKnightStoryState* AKnightStoryGameMode::GetStoryState() const
{
	return Story;
}

const FKnightResult& AKnightStoryGameMode::GetStoryInitializationResult() const
{
	return InitializationResult;
}

void AKnightStoryGameMode::Logout(AController* Exiting)
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

void AKnightStoryGameMode::EndPlay(const EEndPlayReason::Type Reason)
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
