#include "RPGLoop/GolemRPGLoopGameMode.h"

#include "Assets/GolemAssetManager.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Definitions/GolemLootTableDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Loot/GolemItemPickup.h"
#include "Misc/ScopeExit.h"
#include "Party/GolemPartyState.h"
#include "Player/GolemPlayerState.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGCharacter.h"
#include "RPGLoop/GolemRPGGameState.h"
#include "RPGLoop/GolemRPGHUD.h"
#include "RPGLoop/GolemRPGPlayerController.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "RPGLoop/GolemRPGUIPlayerController.h"
#include "RPGLoop/GolemRPGUpgradeStation.h"
#include "SkillLab/GolemSkillLabGameMode.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemRPGLoop, Log, All);

AGolemRPGLoopGameMode::AGolemRPGLoopGameMode()
{
	GolemCharacterClass = AGolemRPGCharacter::StaticClass();
	GameStateClass = AGolemRPGGameState::StaticClass();
	PlayerControllerClass = AGolemRPGUIPlayerController::StaticClass();
	HUDClass = AGolemRPGHUD::StaticClass();
	RPGInitializationResult = FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGComposition"),
		NSLOCTEXT("KnightRPG", "CompositionPending", "The host's RPG domains are not ready."));
}

void AGolemRPGLoopGameMode::InitGameState()
{
	Super::InitGameState();
	RPGInitializationResult = GetArenaInitializationResult();
	if (!HasAuthority() || !RPGInitializationResult.IsSuccess())
	{
		return;
	}
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	UGolemDefinition* Resolved = nullptr;
	RPGInitializationResult = Manager != nullptr
		? Manager->ResolveDefinition(RewardTableId, UGolemLootTableDefinition::StaticClass(), Resolved)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGDefinitionCatalog"), FText::GetEmpty());
	if (!RPGInitializationResult.IsSuccess())
	{
		return;
	}
	RewardTable = CastChecked<UGolemLootTableDefinition>(Resolved);
	RPGInitializationResult = RewardTable->ValidateLootTable();
	if (!RPGInitializationResult.IsSuccess())
	{
		return;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	RPGState = GetWorld()->SpawnActor<AGolemRPGStateActor>(Parameters);
	if (RPGState == nullptr)
	{
		RPGInitializationResult = FGolemResult::Failure(EGolemError::InvalidState,
			TEXT("RPGStateSpawn"), FText::GetEmpty());
		return;
	}
	TArray<FGolemRPGGolemRegistration> Registrations;
	for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		FGolemRPGGolemRegistration Registration;
		Registration.GolemId = Entry.GolemId;
		Registration.HeroDefinitionId = Entry.HeroDefinitionId;
		Registration.Pawn = Entry.Golem;
		Registration.AbilitySystem = Entry.Golem->GetGolemAbilitySystem();
		Registrations.Add(Registration);
	}
	RPGInitializationResult = InitializeRPGDomains(RPGState->GetInventory(), RPGState->GetRPG(), Registrations);
	if (RPGInitializationResult.IsSuccess())
	{
		AGolemRPGGameState* State = GetGameState<AGolemRPGGameState>();
		RPGInitializationResult = State != nullptr ? State->SetRPGState(RPGState)
			: FGolemResult::Failure(EGolemError::TypeMismatch, TEXT("RPGGameState"), FText::GetEmpty());
	}
	if (!RPGInitializationResult.IsSuccess())
	{
		RPGState->Destroy();
		RPGState = nullptr;
	}
	RewardId = FGuid::NewGuid();
}

FGolemResult AGolemRPGLoopGameMode::ConfigureRPGDomains(UGolemInventoryComponent* Inventory,
	UGolemRPGComponent* RPG)
{
	return FGolemResult::Success();
}

FGolemResult AGolemRPGLoopGameMode::InitializeRPGDomains(UGolemInventoryComponent* Inventory,
	UGolemRPGComponent* RPG, TConstArrayView<FGolemRPGGolemRegistration> Registrations)
{
	FGolemResult Result = Inventory->InitializeInventory(GetInitialInventoryCapacity());
	if (Result.IsSuccess())
	{
		Result = ConfigureRPGDomains(Inventory, RPG);
	}
	return Result.IsSuccess() ? RPG->InitializeRPG(Inventory, SkillTreeId, Registrations, 2) : Result;
}

int32 AGolemRPGLoopGameMode::GetInitialInventoryCapacity() const
{
	return 64;
}

bool AGolemRPGLoopGameMode::ShouldIssueLegacyEncounterRewards() const
{
	return true;
}

void AGolemRPGLoopGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (RPGInitializationResult.IsSuccess())
	{
		int32 StationCount = 0;
		for (TActorIterator<AGolemRPGUpgradeStation> Iterator(GetWorld()); Iterator; ++Iterator)
		{
			UpgradeStation = *Iterator;
			++StationCount;
		}
		if (StationCount != 1)
		{
			RPGInitializationResult = FGolemResult::Failure(EGolemError::InvalidData, TEXT("RPGUpgradeStation"),
				NSLOCTEXT("KnightRPG", "StationCount", "The RPG map must contain one valid upgrade station."));
		}
	}
	if (!RPGInitializationResult.IsSuccess())
	{
		UE_LOG(LogGolemRPGLoop, Error, TEXT("RPG composition failed [%s]: %s"),
			*RPGInitializationResult.Context.ToString(), *RPGInitializationResult.Message.ToString());
		return;
	}
	ArenaSnapshotHandle = GetArenaState()->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleArenaSnapshot);
	HandleArenaSnapshot(GetArenaState()->GetSnapshot());
}

FGolemResult AGolemRPGLoopGameMode::BuildCaller(APlayerController* Player,
	FGolemRPGCallerContext& OutCaller) const
{
	OutCaller = FGolemRPGCallerContext();
	const AGolemPartyState* Party = GetPartyState();
	if (!HasAuthority() || bEndingRPGPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown
		|| !IsValid(Player) || Player->IsActorBeingDestroyed() || Player->GetWorld() != GetWorld())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("RPGCaller"), FText::GetEmpty());
	}
	if (!RPGInitializationResult.IsSuccess() || !IsValid(RPGState) || Party == nullptr
		|| !Party->IsCanonicalRosterReady())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGCaller"),
			NSLOCTEXT("KnightRPG", "DomainsPending", "The campaign's party or RPG domains are unavailable."));
	}
	const int32 Slot = Party->GetControlledSlot(Player);
	const FGolemRosterEntry* Entry = Party->FindSlot(Slot);
	const AGolemPlayerState* Human = Player->GetPlayerState<AGolemPlayerState>();
	AGolemCharacter* Golem = Cast<AGolemCharacter>(Player->GetPawn());
	if (Entry == nullptr || Human == nullptr || !Human->GetHumanId().IsValid()
		|| Human->GetAssignedSlot() != Slot || Entry->OwnerKind != EGolemControllerOwner::Human
		|| Entry->HumanPlayerState != Human || Entry->HumanId != Human->GetHumanId()
		|| Entry->Controller != Player || !IsValid(Golem) || Entry->Golem != Golem
		|| Golem->GetController() != Player || Entry->GolemId != Golem->GetGolemEntityId())
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("RPGCaller"),
			NSLOCTEXT("KnightRPG", "CallerOwnership", "Only the human owning a canonical Golem may submit this command."));
	}
	OutCaller.HumanId = Human->GetHumanId();
	OutCaller.GolemId = Entry->GolemId;
	OutCaller.HeroDefinitionId = Entry->HeroDefinitionId;
	OutCaller.Pawn = Golem;
	OutCaller.AbilitySystem = Golem->GetGolemAbilitySystem();
	OutCaller.bInSafeZone = IsInUpgradeSafeZone(Golem);
	OutCaller.bInCombat = GetArenaState() != nullptr && GetArenaState()->GetSnapshot().AliveEnemies != 0;
	if (RPGState->GetRPG()->GetView().IsItemizationEnabled())
	{
		for (const FGolemRosterEntry& Candidate : Party->GetRoster())
		{
			if (Candidate.GolemId == OutCaller.GolemId || Candidate.OwnerKind == EGolemControllerOwner::Companion)
			{
				OutCaller.EditableGolems.Add(Candidate.GolemId);
			}
		}
	}
	if (const AGolemSkillLabGameMode* Lab = Cast<AGolemSkillLabGameMode>(this))
	{
		OutCaller.EncounterId = Lab->GetLabEncounterId();
	}
	return FGolemResult::Success();
}

FGolemRPGCommandResult AGolemRPGLoopGameMode::ExecuteRPGCommand(AGolemRPGPlayerController* Player,
	const FGolemRPGCommand& Command)
{
	FGolemRPGCommandResult Outcome;
	Outcome.Operation = Command.Operation;
	Outcome.RequestId = Command.RequestId;
	Outcome.GolemId = Command.ExpectedGolemId;
	Outcome.InventoryId = Command.InventoryId;
	Outcome.Result = Command.ValidateShape();
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	if (bIssuingReward)
	{
		Outcome.Result = FGolemResult::Failure(EGolemError::Conflict, TEXT("RPGRewardPublication"),
			NSLOCTEXT("KnightRPG", "RewardPublishing", "The encounter reward is still being published."));
		return Outcome;
	}
	FGolemRPGCallerContext Caller;
	Outcome.Result = BuildCaller(Player, Caller);
	if (!Outcome.Result.IsSuccess())
	{
		return Outcome;
	}
	// Authenticate the current human before returning an exact receipt, even after a Golem switch.
	const FGolemRPGCommandResult Committed = RPGState->GetRPG()->QueryCommittedCommand(Caller, Command);
	if (Committed.Result.Error != EGolemError::NotFound)
	{
		return Committed;
	}
	if (RPGState->GetRPG()->GetView().IsItemizationEnabled() && Command.ExpectedGolemId != Caller.GolemId)
	{
		bool bCompanionGearOperation = false;
		switch (Command.Operation)
		{
		case EGolemRPGOperation::Equip:
		case EGolemRPGOperation::Unequip:
		case EGolemRPGOperation::TransferEquipment:
		case EGolemRPGOperation::SwapRings:
		case EGolemRPGOperation::AttachWeaponRune:
		case EGolemRPGOperation::DetachWeaponRune:
		case EGolemRPGOperation::ApplyBuild:
		case EGolemRPGOperation::BuyItem:
		case EGolemRPGOperation::SellItem:
		case EGolemRPGOperation::UpgradeItem:
		case EGolemRPGOperation::BindConsumable:
		case EGolemRPGOperation::ApplyGearPreset:
		case EGolemRPGOperation::ClaimReward:
			bCompanionGearOperation = true;
			break;
		default: break;
		}
		const FGolemRosterEntry* Selected = GetPartyState()->GetRoster().FindByPredicate(
			[&Command](const FGolemRosterEntry& Candidate)
			{
				return Candidate.GolemId == Command.ExpectedGolemId;
			});
		if (!bCompanionGearOperation || Selected == nullptr
			|| Selected->OwnerKind != EGolemControllerOwner::Companion
			|| !Caller.EditableGolems.Contains(Selected->GolemId) || !IsValid(Selected->Golem))
		{
			Outcome.Result = FGolemResult::Failure(EGolemError::NotOwned, TEXT("GearSelectedGolem"),
				NSLOCTEXT("KnightRPG", "SelectedGearPermission", "Only this human's permitted companion gear may be changed."));
			return Outcome;
		}
		// Human authentication stays intact; only the selected canonical gameplay participant changes.
		Caller.GolemId = Selected->GolemId;
		Caller.HeroDefinitionId = Selected->HeroDefinitionId;
		Caller.Pawn = Selected->Golem;
		Caller.AbilitySystem = Selected->Golem->GetGolemAbilitySystem();
		Caller.bInSafeZone &= IsInUpgradeSafeZone(Selected->Golem);
	}
	if (RPGState->GetRPG()->GetView().IsItemizationEnabled()
		&& Command.Operation == EGolemRPGOperation::TransferEquipment)
	{
		const FGolemRosterEntry* Target = GetPartyState()->GetRoster().FindByPredicate(
			[&Command](const FGolemRosterEntry& Candidate)
			{
				return Candidate.GolemId == Command.TargetGolemId;
			});
		if (Target == nullptr || !Caller.EditableGolems.Contains(Target->GolemId) || !IsValid(Target->Golem))
		{
			Outcome.Result = FGolemResult::Failure(EGolemError::NotOwned, TEXT("GearTransferTarget"),
				NSLOCTEXT("KnightRPG", "TransferTargetPermission", "Transfer requires permission for both canonical Golems."));
			return Outcome;
		}
		Caller.bInSafeZone &= IsInUpgradeSafeZone(Target->Golem);
	}
	Outcome = RPGState->GetRPG()->ExecuteCommand(Caller, Command);
	if (Outcome.Result.IsSuccess() && !Outcome.bWasReplay && !bEndingRPGPlay && IsValid(RPGState)
		&& Command.Operation != EGolemRPGOperation::Pickup
		&& RPGState->GetLoopSnapshot().Stage == EGolemRPGLoopStage::RewardAvailable)
	{
		FGolemRPGLoopSnapshot Candidate = RPGState->GetLoopSnapshot();
		Candidate.Stage = EGolemRPGLoopStage::Upgrade;
		const FGolemResult Transition = RPGState->CommitLoopSnapshot(Candidate, Candidate.Revision);
		if (!Transition.IsSuccess())
		{
			UE_LOG(LogGolemRPGLoop, Error, TEXT("Committed RPG command lost its loop presentation: %s"),
				*Transition.Context.ToString());
		}
	}
	return Outcome;
}

bool AGolemRPGLoopGameMode::IsInUpgradeSafeZone(const AActor* Golem) const
{
	if (!IsValid(Golem) || Golem->GetWorld() != GetWorld() || !IsValid(UpgradeStation)
		|| GetArenaState() == nullptr || GetArenaState()->GetSnapshot().AliveEnemies != 0)
	{
		return false;
	}
	const UGolemLifeComponent* Life = Golem->FindComponentByClass<UGolemLifeComponent>();
	if (Life == nullptr || !Life->IsAlive())
	{
		return false;
	}
	const EGolemArenaStage Stage = GetArenaState()->GetSnapshot().Stage;
	if (Stage != EGolemArenaStage::Victory && Stage != EGolemArenaStage::AwaitingStart)
	{
		return false;
	}
	if (FVector::DistSquared(Golem->GetActorLocation(), UpgradeStation->GetActorLocation())
		> FMath::Square(UpgradeStation->GetInteractionRange()))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GolemRPGSafeZone), false, Golem);
	Query.AddIgnoredActor(UpgradeStation);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Golem->GetActorLocation(),
		UpgradeStation->GetActorLocation() + FVector(0.0, 0.0, 90.0), ECC_Visibility, Query);
}

void AGolemRPGLoopGameMode::HandleArenaSnapshot(const FGolemPartyArenaSnapshot& Snapshot)
{
	if (bEndingRPGPlay || RPGState == nullptr || Snapshot.Stage != EGolemArenaStage::Victory)
	{
		return;
	}
	if (RPGState->GetLoopSnapshot().Stage == EGolemRPGLoopStage::EncounterA)
	{
		const FGolemResult Reward = IssueEncounterReward();
		if (!Reward.IsSuccess())
		{
			UE_LOG(LogGolemRPGLoop, Error, TEXT("Actual encounter reward failed [%s]: %s"),
				*Reward.Context.ToString(), *Reward.Message.ToString());
		}
	}
	else if (RPGState->GetLoopSnapshot().Stage == EGolemRPGLoopStage::EncounterB)
	{
		FGolemRPGLoopSnapshot Candidate = RPGState->GetLoopSnapshot();
		Candidate.Stage = EGolemRPGLoopStage::Complete;
		const FGolemResult Commit = RPGState->CommitLoopSnapshot(Candidate, Candidate.Revision);
		if (!Commit.IsSuccess())
		{
			UE_LOG(LogGolemRPGLoop, Error, TEXT("RPG loop completion failed: %s"), *Commit.Context.ToString());
		}
	}
}

FGolemResult AGolemRPGLoopGameMode::IssueEncounterReward()
{
	if (!ShouldIssueLegacyEncounterRewards())
	{
		return FGolemResult::Success();
	}
	if (bIssuingReward || bEndingRPGPlay || RPGState == nullptr || !IsValid(RewardTable)
		|| !IsValid(UpgradeStation) || !RewardId.IsValid()
		|| GetArenaState()->GetSnapshot().Stage != EGolemArenaStage::Victory)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGReward"), FText::GetEmpty());
	}
	if (RPGState->GetLoopSnapshot().bRewardIssued)
	{
		return FGolemResult::Success();
	}
	bIssuingReward = true;
	ON_SCOPE_EXIT
	{
		bIssuingReward = false;
	};
	TArray<FGolemItemInstance> Drops;
	FGolemResult Result = RewardTable->GenerateLoot(RewardSeed, RewardId, Drops);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	TArray<TObjectPtr<AGolemItemPickup>> Spawned;
	TArray<TObjectPtr<AGolemItemPickup>> Registered;
	for (int32 Index = 0; Index < Drops.Num(); ++Index)
	{
		const FVector Position = UpgradeStation->GetActorLocation()
			+ FVector(150.0, (Index - (Drops.Num() - 1) * 0.5) * 110.0, 55.0);
		const FTransform Transform(Position);
		AGolemItemPickup* Pickup = GetWorld()->SpawnActorDeferred<AGolemItemPickup>(
			AGolemItemPickup::StaticClass(), Transform, this, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Pickup == nullptr)
		{
			Result = FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGPickupSpawn"), FText::GetEmpty());
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
		FGolemRPGLoopSnapshot Candidate = RPGState->GetLoopSnapshot();
		Candidate.Stage = EGolemRPGLoopStage::RewardAvailable;
		Candidate.bRewardIssued = true;
		Candidate.RewardPickupCount = Spawned.Num();
		Result = RPGState->CommitLoopSnapshot(Candidate, Candidate.Revision);
	}
	if (!Result.IsSuccess())
	{
		for (AGolemItemPickup* Pickup : Registered)
		{
			if (IsValid(RPGState) && IsValid(Pickup))
			{
				const FGolemResult Cleanup = RPGState->GetRPG()->UnregisterUnclaimedPickup(Pickup);
				if (!Cleanup.IsSuccess())
				{
					UE_LOG(LogGolemRPGLoop, Error, TEXT("Reward rollback could not release pickup %s: %s"),
						*Pickup->GetPickupId().ToString(), *Cleanup.Context.ToString());
				}
			}
		}
		for (AGolemItemPickup* Pickup : Spawned)
		{
			if (IsValid(Pickup))
			{
				Pickup->Destroy();
			}
		}
		return Result;
	}
	RewardPickups = MoveTemp(Spawned);
	UE_LOG(LogGolemRPGLoop, Display, TEXT("Real encounter A reward %s generated %d pickups once."),
		*RewardId.ToString(), RewardPickups.Num());
	return Result;
}

FGolemResult AGolemRPGLoopGameMode::TryInteract(APlayerController* Player)
{
	FGolemRPGCallerContext Caller;
	const FGolemResult Authorized = BuildCaller(Player, Caller);
	if (!Authorized.IsSuccess())
	{
		return Authorized;
	}
	if (GetArenaState()->GetSnapshot().Stage == EGolemArenaStage::Victory
		&& RPGState->GetLoopSnapshot().Stage == EGolemRPGLoopStage::EncounterA)
	{
		return IssueEncounterReward();
	}
	AGolemItemPickup* Nearest = nullptr;
	double BestDistance = FMath::Square(static_cast<double>(GolemRPG::PickupRange));
	for (TActorIterator<AGolemItemPickup> Iterator(GetWorld()); Iterator; ++Iterator)
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
		FGolemRPGCommand Command;
		Command.Operation = EGolemRPGOperation::Pickup;
		Command.RequestId = FGuid::NewGuid();
		Command.ExpectedGolemId = Caller.GolemId;
		const FGolemInventoryView Inventory = RPGState->GetInventory()->GetView();
		Command.InventoryId = Inventory.InventoryId;
		Command.ExpectedInventoryRevision = Inventory.Revision;
		Command.ExpectedGolemRevision = RPGState->GetRPG()->GetGolemView(Caller.GolemId).Revision;
		Command.PickupId = Nearest->GetPickupId();
		return ExecuteRPGCommand(Cast<AGolemRPGPlayerController>(Player), Command).Result;
	}
	return Super::TryInteract(Player);
}

FGolemResult AGolemRPGLoopGameMode::TryContinueRPGLoop(APlayerController* Player)
{
	FGolemRPGCallerContext Caller;
	FGolemResult Result = BuildCaller(Player, Caller);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	const EGolemRPGLoopStage Stage = RPGState->GetLoopSnapshot().Stage;
	if (!Player->IsLocalController() || !Caller.bInSafeZone
		|| (Stage != EGolemRPGLoopStage::RewardAvailable && Stage != EGolemRPGLoopStage::Upgrade)
		|| RPGState->GetRPG()->IsTransactionInProgress())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGContinue"),
			NSLOCTEXT("KnightRPG", "ContinuePolicy", "The host must upgrade the party, then Continue at the safe station."));
	}
	bool bHasUpgrade = false;
	for (const FGolemRPGGolemView& Golem : RPGState->GetRPG()->GetView().Golems)
	{
		bHasUpgrade |= !Golem.Assignments.IsEmpty() || !Golem.PurchasedSkills.IsEmpty();
	}
	if (!bHasUpgrade)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RPGContinue"),
			NSLOCTEXT("KnightRPG", "UpgradeRequired", "Equip gear, equip a rune or purchase a skill before encounter B."));
	}
	Result = Super::TryRestartDrill(Player);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FGolemRPGLoopSnapshot Candidate = RPGState->GetLoopSnapshot();
	Candidate.Stage = EGolemRPGLoopStage::EncounterB;
	return RPGState->CommitLoopSnapshot(Candidate, Candidate.Revision);
}

FGolemResult AGolemRPGLoopGameMode::TryRestartDrill(APlayerController* Player)
{
	FGolemRPGCallerContext Caller;
	const FGolemResult Authorized = BuildCaller(Player, Caller);
	if (!Authorized.IsSuccess())
	{
		return Authorized;
	}
	const FGolemResult Reset = Super::TryRestartDrill(Player);
	if (Reset.IsSuccess() && RPGState->GetLoopSnapshot().Stage == EGolemRPGLoopStage::Complete)
	{
		FGolemRPGLoopSnapshot Candidate = RPGState->GetLoopSnapshot();
		Candidate.Stage = EGolemRPGLoopStage::EncounterB;
		return RPGState->CommitLoopSnapshot(Candidate, Candidate.Revision);
	}
	return Reset;
}

AGolemRPGStateActor* AGolemRPGLoopGameMode::GetRPGState() const
{
	return RPGState;
}

AGolemRPGUpgradeStation* AGolemRPGLoopGameMode::GetUpgradeStation() const
{
	return UpgradeStation;
}

const FGolemResult& AGolemRPGLoopGameMode::GetRPGInitializationResult() const
{
	return RPGInitializationResult;
}

void AGolemRPGLoopGameMode::EndPlay(EEndPlayReason::Type EndPlayReason)
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
