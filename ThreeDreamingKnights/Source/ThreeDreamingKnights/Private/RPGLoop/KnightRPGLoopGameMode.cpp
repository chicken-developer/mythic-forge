#include "RPGLoop/KnightRPGLoopGameMode.h"

#include "Assets/KnightAssetManager.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Definitions/KnightLootTableDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Loot/KnightItemPickup.h"
#include "Misc/ScopeExit.h"
#include "Party/KnightPartyState.h"
#include "Player/KnightPlayerState.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGCharacter.h"
#include "RPGLoop/KnightRPGGameState.h"
#include "RPGLoop/KnightRPGHUD.h"
#include "RPGLoop/KnightRPGPlayerController.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "RPGLoop/KnightRPGUIPlayerController.h"
#include "RPGLoop/KnightRPGUpgradeStation.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightRPGLoop, Log, All);

AKnightRPGLoopGameMode::AKnightRPGLoopGameMode()
{
	KnightCharacterClass = AKnightRPGCharacter::StaticClass();
	GameStateClass = AKnightRPGGameState::StaticClass();
	PlayerControllerClass = AKnightRPGUIPlayerController::StaticClass();
	HUDClass = AKnightRPGHUD::StaticClass();
	RPGInitializationResult = FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGComposition"),
		NSLOCTEXT("KnightRPG", "CompositionPending", "The host's RPG domains are not ready."));
}

void AKnightRPGLoopGameMode::InitGameState()
{
	Super::InitGameState();
	RPGInitializationResult = GetArenaInitializationResult();
	if (!HasAuthority() || !RPGInitializationResult.IsSuccess())
	{
		return;
	}
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	UKnightDefinition* Resolved = nullptr;
	RPGInitializationResult = Manager != nullptr
		? Manager->ResolveDefinition(RewardTableId, UKnightLootTableDefinition::StaticClass(), Resolved)
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGDefinitionCatalog"), FText::GetEmpty());
	if (!RPGInitializationResult.IsSuccess())
	{
		return;
	}
	RewardTable = CastChecked<UKnightLootTableDefinition>(Resolved);
	RPGInitializationResult = RewardTable->ValidateLootTable();
	if (!RPGInitializationResult.IsSuccess())
	{
		return;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	RPGState = GetWorld()->SpawnActor<AKnightRPGStateActor>(Parameters);
	if (RPGState == nullptr)
	{
		RPGInitializationResult = FKnightResult::Failure(EKnightError::InvalidState,
			TEXT("RPGStateSpawn"), FText::GetEmpty());
		return;
	}
	RPGInitializationResult = RPGState->GetInventory()->InitializeInventory(64);
	if (RPGInitializationResult.IsSuccess())
	{
		TArray<FKnightRPGKnightRegistration> Registrations;
		for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
		{
			FKnightRPGKnightRegistration Registration;
			Registration.KnightId = Entry.KnightId;
			Registration.HeroDefinitionId = Entry.HeroDefinitionId;
			Registration.Pawn = Entry.Knight;
			Registration.AbilitySystem = Entry.Knight->GetKnightAbilitySystem();
			Registrations.Add(Registration);
		}
		RPGInitializationResult = RPGState->GetRPG()->InitializeRPG(
			RPGState->GetInventory(), SkillTreeId, Registrations, 2);
	}
	if (RPGInitializationResult.IsSuccess())
	{
		AKnightRPGGameState* State = GetGameState<AKnightRPGGameState>();
		RPGInitializationResult = State != nullptr ? State->SetRPGState(RPGState)
			: FKnightResult::Failure(EKnightError::TypeMismatch, TEXT("RPGGameState"), FText::GetEmpty());
	}
	if (!RPGInitializationResult.IsSuccess())
	{
		RPGState->Destroy();
		RPGState = nullptr;
	}
	RewardId = FGuid::NewGuid();
}

void AKnightRPGLoopGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (RPGInitializationResult.IsSuccess())
	{
		int32 StationCount = 0;
		for (TActorIterator<AKnightRPGUpgradeStation> Iterator(GetWorld()); Iterator; ++Iterator)
		{
			UpgradeStation = *Iterator;
			++StationCount;
		}
		if (StationCount != 1)
		{
			RPGInitializationResult = FKnightResult::Failure(EKnightError::InvalidData, TEXT("RPGUpgradeStation"),
				NSLOCTEXT("KnightRPG", "StationCount", "The RPG map must contain one valid upgrade station."));
		}
	}
	if (!RPGInitializationResult.IsSuccess())
	{
		UE_LOG(LogKnightRPGLoop, Error, TEXT("RPG composition failed [%s]: %s"),
			*RPGInitializationResult.Context.ToString(), *RPGInitializationResult.Message.ToString());
		return;
	}
	ArenaSnapshotHandle = GetArenaState()->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleArenaSnapshot);
	HandleArenaSnapshot(GetArenaState()->GetSnapshot());
}

FKnightResult AKnightRPGLoopGameMode::BuildCaller(APlayerController* Player,
	FKnightRPGCallerContext& OutCaller) const
{
	OutCaller = FKnightRPGCallerContext();
	const AKnightPartyState* Party = GetPartyState();
	if (!HasAuthority() || bEndingRPGPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown
		|| !IsValid(Player) || Player->IsActorBeingDestroyed() || Player->GetWorld() != GetWorld())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("RPGCaller"), FText::GetEmpty());
	}
	if (!RPGInitializationResult.IsSuccess() || !IsValid(RPGState) || Party == nullptr
		|| !Party->IsCanonicalRosterReady())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGCaller"),
			NSLOCTEXT("KnightRPG", "DomainsPending", "The campaign's party or RPG domains are unavailable."));
	}
	const int32 Slot = Party->GetControlledSlot(Player);
	const FKnightRosterEntry* Entry = Party->FindSlot(Slot);
	const AKnightPlayerState* Human = Player->GetPlayerState<AKnightPlayerState>();
	AKnightCharacter* Knight = Cast<AKnightCharacter>(Player->GetPawn());
	if (Entry == nullptr || Human == nullptr || !Human->GetHumanId().IsValid()
		|| Human->GetAssignedSlot() != Slot || Entry->OwnerKind != EKnightControllerOwner::Human
		|| Entry->HumanPlayerState != Human || Entry->HumanId != Human->GetHumanId()
		|| Entry->Controller != Player || !IsValid(Knight) || Entry->Knight != Knight
		|| Knight->GetController() != Player || Entry->KnightId != Knight->GetKnightEntityId())
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("RPGCaller"),
			NSLOCTEXT("KnightRPG", "CallerOwnership", "Only the human owning a canonical Knight may submit this command."));
	}
	OutCaller.HumanId = Human->GetHumanId();
	OutCaller.KnightId = Entry->KnightId;
	OutCaller.HeroDefinitionId = Entry->HeroDefinitionId;
	OutCaller.Pawn = Knight;
	OutCaller.AbilitySystem = Knight->GetKnightAbilitySystem();
	OutCaller.bInSafeZone = IsInUpgradeSafeZone(Knight);
	return FKnightResult::Success();
}

FKnightRPGCommandResult AKnightRPGLoopGameMode::ExecuteRPGCommand(AKnightRPGPlayerController* Player,
	const FKnightRPGCommand& Command)
{
	FKnightRPGCommandResult Outcome;
	Outcome.Operation = Command.Operation;
	Outcome.RequestId = Command.RequestId;
	Outcome.KnightId = Command.ExpectedKnightId;
	Outcome.InventoryId = Command.InventoryId;
	Outcome.Result = Command.ValidateShape();
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	if (bIssuingReward)
	{
		Outcome.Result = FKnightResult::Failure(EKnightError::Conflict, TEXT("RPGRewardPublication"),
			NSLOCTEXT("KnightRPG", "RewardPublishing", "The encounter reward is still being published."));
		return Outcome;
	}
	FKnightRPGCallerContext Caller;
	Outcome.Result = BuildCaller(Player, Caller);
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	// The authenticated domain can return an exact historical receipt after a legitimate Knight switch.
	Outcome = RPGState->GetRPG()->ExecuteCommand(Caller, Command);
	if (Outcome.Result.IsSuccess() && !Outcome.bWasReplay && !bEndingRPGPlay && IsValid(RPGState)
		&& Command.Operation != EKnightRPGOperation::Pickup
		&& RPGState->GetLoopSnapshot().Stage == EKnightRPGLoopStage::RewardAvailable)
	{
		FKnightRPGLoopSnapshot Candidate = RPGState->GetLoopSnapshot();
		Candidate.Stage = EKnightRPGLoopStage::Upgrade;
		const FKnightResult Transition = RPGState->CommitLoopSnapshot(Candidate, Candidate.Revision);
		if (!Transition.IsSuccess())
		{
			UE_LOG(LogKnightRPGLoop, Error, TEXT("Committed RPG command lost its loop presentation: %s"),
				*Transition.Context.ToString());
		}
	}
	return Outcome;
}

bool AKnightRPGLoopGameMode::IsInUpgradeSafeZone(const AActor* Knight) const
{
	if (!IsValid(Knight) || Knight->GetWorld() != GetWorld() || !IsValid(UpgradeStation)
		|| GetArenaState() == nullptr || GetArenaState()->GetSnapshot().AliveEnemies != 0)
	{
		return false;
	}
	const UKnightLifeComponent* Life = Knight->FindComponentByClass<UKnightLifeComponent>();
	if (Life == nullptr || !Life->IsAlive())
	{
		return false;
	}
	const EKnightArenaStage Stage = GetArenaState()->GetSnapshot().Stage;
	if (Stage != EKnightArenaStage::Victory && Stage != EKnightArenaStage::AwaitingStart)
	{
		return false;
	}
	if (FVector::DistSquared(Knight->GetActorLocation(), UpgradeStation->GetActorLocation())
		> FMath::Square(UpgradeStation->GetInteractionRange()))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(KnightRPGSafeZone), false, Knight);
	Query.AddIgnoredActor(UpgradeStation);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Knight->GetActorLocation(),
		UpgradeStation->GetActorLocation() + FVector(0.0, 0.0, 90.0), ECC_Visibility, Query);
}

void AKnightRPGLoopGameMode::HandleArenaSnapshot(const FKnightPartyArenaSnapshot& Snapshot)
{
	if (bEndingRPGPlay || RPGState == nullptr || Snapshot.Stage != EKnightArenaStage::Victory)
	{
		return;
	}
	if (RPGState->GetLoopSnapshot().Stage == EKnightRPGLoopStage::EncounterA)
	{
		const FKnightResult Reward = IssueEncounterReward();
		if (!Reward.IsSuccess())
		{
			UE_LOG(LogKnightRPGLoop, Error, TEXT("Actual encounter reward failed [%s]: %s"),
				*Reward.Context.ToString(), *Reward.Message.ToString());
		}
	}
	else if (RPGState->GetLoopSnapshot().Stage == EKnightRPGLoopStage::EncounterB)
	{
		FKnightRPGLoopSnapshot Candidate = RPGState->GetLoopSnapshot();
		Candidate.Stage = EKnightRPGLoopStage::Complete;
		const FKnightResult Commit = RPGState->CommitLoopSnapshot(Candidate, Candidate.Revision);
		if (!Commit.IsSuccess())
		{
			UE_LOG(LogKnightRPGLoop, Error, TEXT("RPG loop completion failed: %s"), *Commit.Context.ToString());
		}
	}
}

FKnightResult AKnightRPGLoopGameMode::IssueEncounterReward()
{
	if (bIssuingReward || bEndingRPGPlay || RPGState == nullptr || !IsValid(RewardTable)
		|| !IsValid(UpgradeStation) || !RewardId.IsValid()
		|| GetArenaState()->GetSnapshot().Stage != EKnightArenaStage::Victory)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGReward"), FText::GetEmpty());
	}
	if (RPGState->GetLoopSnapshot().bRewardIssued)
	{
		return FKnightResult::Success();
	}
	bIssuingReward = true;
	ON_SCOPE_EXIT
	{
		bIssuingReward = false;
	};
	TArray<FKnightItemInstance> Drops;
	FKnightResult Result = RewardTable->GenerateLoot(RewardSeed, RewardId, Drops);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	TArray<TObjectPtr<AKnightItemPickup>> Spawned;
	TArray<TObjectPtr<AKnightItemPickup>> Registered;
	for (int32 Index = 0; Index < Drops.Num(); ++Index)
	{
		const FVector Position = UpgradeStation->GetActorLocation()
			+ FVector(150.0, (Index - (Drops.Num() - 1) * 0.5) * 110.0, 55.0);
		const FTransform Transform(Position);
		AKnightItemPickup* Pickup = GetWorld()->SpawnActorDeferred<AKnightItemPickup>(
			AKnightItemPickup::StaticClass(), Transform, this, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Pickup == nullptr)
		{
			Result = FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGPickupSpawn"), FText::GetEmpty());
			break;
		}
		Spawned.Add(Pickup);
		Result = Pickup->InitializePickup(Drops[Index].InstanceId, Drops[Index]);
		Pickup->FinishSpawning(Transform);
		if (Result.IsSuccess())
		{
			Result = RPGState->GetRPG()->RegisterPickup(Pickup);
			if (Result.IsSuccess())
			{
				Registered.Add(Pickup);
			}
		}
		if (!Result.IsSuccess())
		{
			break;
		}
	}
	if (Result.IsSuccess())
	{
		FKnightRPGLoopSnapshot Candidate = RPGState->GetLoopSnapshot();
		Candidate.Stage = EKnightRPGLoopStage::RewardAvailable;
		Candidate.bRewardIssued = true;
		Candidate.RewardPickupCount = Spawned.Num();
		Result = RPGState->CommitLoopSnapshot(Candidate, Candidate.Revision);
	}
	if (!Result.IsSuccess())
	{
		for (AKnightItemPickup* Pickup : Registered)
		{
			if (IsValid(RPGState) && IsValid(Pickup))
			{
				const FKnightResult Cleanup = RPGState->GetRPG()->UnregisterUnclaimedPickup(Pickup);
				if (!Cleanup.IsSuccess())
				{
					UE_LOG(LogKnightRPGLoop, Error, TEXT("Reward rollback could not release pickup %s: %s"),
						*Pickup->GetPickupId().ToString(), *Cleanup.Context.ToString());
				}
			}
		}
		for (AKnightItemPickup* Pickup : Spawned)
		{
			if (IsValid(Pickup))
			{
				Pickup->Destroy();
			}
		}
		return Result;
	}
	RewardPickups = MoveTemp(Spawned);
	UE_LOG(LogKnightRPGLoop, Display, TEXT("Real encounter A reward %s generated %d pickups once."),
		*RewardId.ToString(), RewardPickups.Num());
	return Result;
}

FKnightResult AKnightRPGLoopGameMode::TryInteract(APlayerController* Player)
{
	FKnightRPGCallerContext Caller;
	const FKnightResult Authorized = BuildCaller(Player, Caller);
	if (!Authorized.IsSuccess())
	{
		return Authorized;
	}
	if (GetArenaState()->GetSnapshot().Stage == EKnightArenaStage::Victory
		&& RPGState->GetLoopSnapshot().Stage == EKnightRPGLoopStage::EncounterA)
	{
		return IssueEncounterReward();
	}
	AKnightItemPickup* Nearest = nullptr;
	double BestDistance = FMath::Square(static_cast<double>(KnightRPG::PickupRange));
	for (TActorIterator<AKnightItemPickup> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		const double Distance = FVector::DistSquared(Caller.Pawn->GetActorLocation(), Iterator->GetActorLocation());
		if (!Iterator->IsClaimed() && Distance <= BestDistance)
		{
			Nearest = *Iterator;
			BestDistance = Distance;
		}
	}
	if (Nearest != nullptr)
	{
		FKnightRPGCommand Command;
		Command.Operation = EKnightRPGOperation::Pickup;
		Command.RequestId = FGuid::NewGuid();
		Command.ExpectedKnightId = Caller.KnightId;
		const FKnightInventoryView Inventory = RPGState->GetInventory()->GetView();
		Command.InventoryId = Inventory.InventoryId;
		Command.ExpectedInventoryRevision = Inventory.Revision;
		Command.ExpectedKnightRevision = RPGState->GetRPG()->GetKnightView(Caller.KnightId).Revision;
		Command.PickupId = Nearest->GetPickupId();
		return ExecuteRPGCommand(Cast<AKnightRPGPlayerController>(Player), Command).Result;
	}
	return Super::TryInteract(Player);
}

FKnightResult AKnightRPGLoopGameMode::TryContinueRPGLoop(APlayerController* Player)
{
	FKnightRPGCallerContext Caller;
	FKnightResult Result = BuildCaller(Player, Caller);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	const EKnightRPGLoopStage Stage = RPGState->GetLoopSnapshot().Stage;
	if (!Player->IsLocalController() || !Caller.bInSafeZone
		|| (Stage != EKnightRPGLoopStage::RewardAvailable && Stage != EKnightRPGLoopStage::Upgrade)
		|| RPGState->GetRPG()->IsTransactionInProgress())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGContinue"),
			NSLOCTEXT("KnightRPG", "ContinuePolicy", "The host must upgrade the party, then Continue at the safe station."));
	}
	bool bHasUpgrade = false;
	for (const FKnightRPGKnightView& Knight : RPGState->GetRPG()->GetView().Knights)
	{
		bHasUpgrade |= !Knight.Assignments.IsEmpty() || !Knight.PurchasedSkills.IsEmpty();
	}
	if (!bHasUpgrade)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RPGContinue"),
			NSLOCTEXT("KnightRPG", "UpgradeRequired", "Equip gear, equip a rune or purchase a skill before encounter B."));
	}
	Result = Super::TryRestartDrill(Player);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FKnightRPGLoopSnapshot Candidate = RPGState->GetLoopSnapshot();
	Candidate.Stage = EKnightRPGLoopStage::EncounterB;
	return RPGState->CommitLoopSnapshot(Candidate, Candidate.Revision);
}

FKnightResult AKnightRPGLoopGameMode::TryRestartDrill(APlayerController* Player)
{
	FKnightRPGCallerContext Caller;
	const FKnightResult Authorized = BuildCaller(Player, Caller);
	if (!Authorized.IsSuccess())
	{
		return Authorized;
	}
	const FKnightResult Reset = Super::TryRestartDrill(Player);
	if (Reset.IsSuccess() && RPGState->GetLoopSnapshot().Stage == EKnightRPGLoopStage::Complete)
	{
		FKnightRPGLoopSnapshot Candidate = RPGState->GetLoopSnapshot();
		Candidate.Stage = EKnightRPGLoopStage::EncounterB;
		return RPGState->CommitLoopSnapshot(Candidate, Candidate.Revision);
	}
	return Reset;
}

AKnightRPGStateActor* AKnightRPGLoopGameMode::GetRPGState() const
{
	return RPGState;
}

AKnightRPGUpgradeStation* AKnightRPGLoopGameMode::GetUpgradeStation() const
{
	return UpgradeStation;
}

const FKnightResult& AKnightRPGLoopGameMode::GetRPGInitializationResult() const
{
	return RPGInitializationResult;
}

void AKnightRPGLoopGameMode::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	bEndingRPGPlay = true;
	if (IsValid(GetArenaState()))
	{
		GetArenaState()->OnSnapshotChanged.Remove(ArenaSnapshotHandle);
	}
	ArenaSnapshotHandle.Reset();
	RewardPickups.Reset();
	Super::EndPlay(EndPlayReason);
}
