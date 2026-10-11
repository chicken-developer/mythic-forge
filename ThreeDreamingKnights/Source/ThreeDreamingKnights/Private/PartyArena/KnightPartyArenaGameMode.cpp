#include "PartyArena/KnightPartyArenaGameMode.h"

#include "Assets/KnightAssetManager.h"
#include "Boss/KnightBossPhaseComponent.h"
#include "BrainComponent.h"
#include "Combat/KnightCombatDirectorSubsystem.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Context/KnightAIContextComponent.h"
#include "Controllers/KnightArenaCompanionController.h"
#include "Definitions/KnightAIArchetypeDefinition.h"
#include "Definitions/KnightBossPhaseDefinition.h"
#include "Finisher/KnightFinisherComponent.h"
#include "Party/KnightPartyState.h"
#include "PartyArena/KnightArenaCharacter.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "PartyArena/KnightArenaPlayerController.h"
#include "PartyArena/KnightPartyArenaHUD.h"
#include "PartyArena/KnightPartyArenaStartMarker.h"
#include "Traversal/KnightTraversalComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightPartyArena, Log, All);

AKnightPartyArenaGameMode::AKnightPartyArenaGameMode()
{
	KnightCharacterClass = AKnightArenaCharacter::StaticClass();
	AIControllerClass = AKnightArenaCompanionController::StaticClass();
	PlayerControllerClass = AKnightArenaPlayerController::StaticClass();
	HUDClass = AKnightPartyArenaHUD::StaticClass();
	ArenaInitializationResult = FKnightResult::Failure(EKnightError::InvalidState, TEXT("PartyArena"),
		NSLOCTEXT("KnightArena", "ArenaPending", "The party arena is not initialized yet."));
}

void AKnightPartyArenaGameMode::InitGameState()
{
	Super::InitGameState();
	ArenaInitializationResult = GetInitializationResult();
	if (!HasAuthority() || !ArenaInitializationResult.IsSuccess())
	{
		return;
	}
	for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		AKnightArenaCharacter* Knight = Cast<AKnightArenaCharacter>(Entry.Knight);
		if (Knight == nullptr)
		{
			ArenaInitializationResult = FKnightResult::Failure(EKnightError::TypeMismatch,
				TEXT("PartyArenaPawn"), FText::GetEmpty());
			return;
		}
		ArenaInitializationResult = Knight->InitializeArenaCombat();
		if (!ArenaInitializationResult.IsSuccess())
		{
			return;
		}
		if (AKnightArenaCompanionController* Companion = Cast<AKnightArenaCompanionController>(Knight->GetController()))
		{
			ArenaInitializationResult = Companion->RefreshArenaConfiguration();
			if (!ArenaInitializationResult.IsSuccess())
			{
				return;
			}
		}
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	ArenaState = GetWorld()->SpawnActor<AKnightPartyArenaState>(Parameters);
	if (ArenaState == nullptr)
	{
		ArenaInitializationResult = FKnightResult::Failure(EKnightError::InvalidState,
			TEXT("PartyArenaState"), FText::GetEmpty());
	}
}

void AKnightPartyArenaGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (!ArenaInitializationResult.IsSuccess() || ArenaState == nullptr)
	{
		UE_LOG(LogKnightPartyArena, Error, TEXT("Arena initialization failed: %s"),
			*ArenaInitializationResult.Message.ToString());
		return;
	}
	for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		FKnightArenaLifeBinding Binding;
		Binding.Life = Entry.Knight->GetLifeComponent();
		Binding.Handle = Binding.Life->OnLifeStateChanged.AddWeakLambda(this,
			[this](EKnightLifeState Previous, EKnightLifeState Current)
			{
				ScheduleEncounterRefresh();
			});
		PartyBindings.Add(Binding);
	}
	RefreshEncounter();
}

AKnightPartyArenaState* AKnightPartyArenaGameMode::GetArenaState() const
{
	return ArenaState;
}

const FKnightResult& AKnightPartyArenaGameMode::GetArenaInitializationResult() const
{
	return ArenaInitializationResult;
}

const TArray<TObjectPtr<AKnightArenaEnemy>>& AKnightPartyArenaGameMode::GetEncounterEnemies() const
{
	return Enemies;
}

FKnightResult AKnightPartyArenaGameMode::TryInteract(APlayerController* Player)
{
	AKnightPartyState* Party = GetPartyState();
	AKnightCharacter* Knight = IsValid(Player) ? Cast<AKnightCharacter>(Player->GetPawn()) : nullptr;
	if (!HasAuthority() || !IsValid(Player) || Player->GetWorld() != GetWorld())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("StartArena"), FText::GetEmpty());
	}
	if (Party == nullptr || Party->GetControlledSlot(Player) == INDEX_NONE || Knight == nullptr
		|| !Knight->GetLifeComponent()->IsAlive() || !ArenaInitializationResult.IsSuccess() || ArenaState == nullptr)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("StartArena"),
			NSLOCTEXT("KnightArena", "StartOwner", "Control a living canonical Knight to start the arena."));
	}
	if (ArenaState->GetSnapshot().Stage != EKnightArenaStage::AwaitingStart)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("StartArena"),
			NSLOCTEXT("KnightArena", "StartBusy", "The encounter has already started. The host can retry with R."));
	}
	AKnightPartyArenaStartMarker* Marker = nullptr;
	for (TActorIterator<AKnightPartyArenaStartMarker> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		if (Marker != nullptr)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("StartArena"), FText::GetEmpty());
		}
		Marker = *Iterator;
	}
	if (Marker == nullptr || FVector::DistSquared(Knight->GetActorLocation(), Marker->GetActorLocation())
		> FMath::Square(Marker->GetInteractionRange()))
	{
		return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("StartArena"),
			NSLOCTEXT("KnightArena", "StartRange", "Move to the yellow START marker and press F."));
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(KnightArenaStart), false, Knight);
	Query.AddIgnoredActor(Marker);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Knight->GetActorLocation(),
		Marker->GetActorLocation() + FVector(0.0, 0.0, 75.0), ECC_Visibility, Query))
	{
		return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("StartArena"),
			NSLOCTEXT("KnightArena", "StartOccluded", "The START marker must be visible."));
	}
	return SpawnEncounter(EKnightArenaStage::Wave);
}

FKnightResult AKnightPartyArenaGameMode::SpawnEncounter(const EKnightArenaStage Stage)
{
	if (Stage != EKnightArenaStage::Wave && Stage != EKnightArenaStage::Boss)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("ArenaSpawn"), FText::GetEmpty());
	}
	const bool bBoss = Stage == EKnightArenaStage::Boss;
	const FPrimaryAssetId ArchetypeId(TEXT("KnightAIArchetype"), bBoss ? TEXT("ArenaBoss") : TEXT("ArenaEnemy"));
	const FPrimaryAssetId BossId = bBoss
		? FPrimaryAssetId(TEXT("KnightBossPhase"), TEXT("ArenaBoss")) : FPrimaryAssetId();
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	UKnightDefinition* Definition = nullptr;
	FKnightResult Result = Manager != nullptr
		? Manager->ResolveDefinition(ArchetypeId, UKnightAIArchetypeDefinition::StaticClass(), Definition)
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("ArenaSpawn"), FText::GetEmpty());
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Result = CastChecked<UKnightAIArchetypeDefinition>(Definition)->ValidateArchetype();
	if (Result.IsSuccess() && bBoss)
	{
		Result = Manager->ResolveDefinition(BossId, UKnightBossPhaseDefinition::StaticClass(), Definition);
		if (Result.IsSuccess())
		{
			Result = CastChecked<UKnightBossPhaseDefinition>(Definition)->ValidateBossPhases();
		}
	}
	if (!Result.IsSuccess())
	{
		return Result;
	}
	ClearEncounter();
	const TArray<FVector> SpawnPoints = bBoss
		? TArray<FVector>{FVector(0.0, 850.0, 100.0)}
		: (bFixtureElite ? TArray<FVector>{FVector(0.0, 500.0, 100.0)}
			: TArray<FVector>{FVector(-600.0, 250.0, 100.0), FVector(600.0, 250.0, 100.0)});
	for (const FVector& Point : SpawnPoints)
	{
		const FTransform Transform(FRotator(0.0, -90.0, 0.0), Point);
		AKnightArenaEnemy* Enemy = GetWorld()->SpawnActorDeferred<AKnightArenaEnemy>(
			AKnightArenaEnemy::StaticClass(), Transform, this, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (Enemy == nullptr)
		{
			Result = FKnightResult::Failure(EKnightError::InvalidState, TEXT("ArenaSpawn"), FText::GetEmpty());
			break;
		}
		Enemies.Add(Enemy);
		Result = Enemy->InitializeArenaEnemy(ArchetypeId, BossId);
		Enemy->FinishSpawning(Transform);
		if (Result.IsSuccess())
		{
			Result = Enemy->GetInitializationResult();
		}
		if (!Result.IsSuccess())
		{
			break;
		}
		FKnightArenaLifeBinding Binding;
		Binding.Life = Enemy->GetLifeComponent();
		Binding.Handle = Binding.Life->OnLifeStateChanged.AddWeakLambda(this,
			[this](EKnightLifeState Previous, EKnightLifeState Current)
			{
				ScheduleEncounterRefresh();
			});
		EnemyBindings.Add(Binding);
		Enemy->OnEndPlay.AddDynamic(this, &ThisClass::HandleEncounterEnemyEndPlay);
		if (bBoss)
		{
			BossPhaseHandle = Enemy->GetBossPhaseComponent()->OnPhaseChanged.AddUObject(
				this, &ThisClass::HandleBossPhaseChanged);
		}
	}
	if (!Result.IsSuccess())
	{
		ClearEncounter();
		UE_LOG(LogKnightPartyArena, Error, TEXT("Actual encounter spawn failed: %s"), *Result.Message.ToString());
		return Result;
	}
	FKnightPartyArenaSnapshot Snapshot = ArenaState->GetSnapshot();
	Snapshot.Stage = Stage;
	Snapshot.AliveEnemies = Enemies.Num();
	Snapshot.BossPhase = bBoss ? Enemies[0]->GetBossPhaseComponent()->GetPhaseIndex() : INDEX_NONE;
	ArenaState->CommitSnapshot(Snapshot);
	UE_LOG(LogKnightPartyArena, Display, TEXT("Arena actual stage%d enemies%d canonical%d"),
		static_cast<int32>(Stage), Enemies.Num(), GetPartyState()->GetRoster().Num());
	return Result;
}

void AKnightPartyArenaGameMode::ClearEncounter()
{
	for (const FKnightArenaLifeBinding& Binding : EnemyBindings)
	{
		if (Binding.Life.IsValid())
		{
			Binding.Life->OnLifeStateChanged.Remove(Binding.Handle);
		}
	}
	EnemyBindings.Reset();
	for (AKnightArenaEnemy* Enemy : Enemies)
	{
		if (IsValid(Enemy))
		{
			Enemy->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleEncounterEnemyEndPlay);
			Enemy->GetBossPhaseComponent()->OnPhaseChanged.Remove(BossPhaseHandle);
			Enemy->Destroy();
		}
	}
	Enemies.Reset();
	BossPhaseHandle.Reset();
	bUnexpectedEnemyLoss = false;
}

void AKnightPartyArenaGameMode::HandleEncounterEnemyEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	if (bResetting || bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown
		|| Reason == EEndPlayReason::LevelTransition || Reason == EEndPlayReason::EndPlayInEditor
		|| Reason == EEndPlayReason::Quit)
	{
		return;
	}
	AKnightArenaEnemy* Enemy = Cast<AKnightArenaEnemy>(Actor);
	if (Enemy != nullptr && !Enemy->GetLifeComponent()->IsDead())
	{
		bUnexpectedEnemyLoss = true;
		ScheduleEncounterRefresh();
	}
}

void AKnightPartyArenaGameMode::ScheduleEncounterRefresh()
{
	if (bResetting || bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown)
	{
		return;
	}
	if (!GetWorldTimerManager().TimerExists(RefreshTimer))
	{
		RefreshTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::RefreshEncounter);
	}
}

void AKnightPartyArenaGameMode::RefreshEncounter()
{
	RefreshTimer.Invalidate();
	if (bResetting || bEndingPlay || ArenaState == nullptr || GetPartyState() == nullptr)
	{
		return;
	}
	FKnightPartyArenaSnapshot Snapshot = ArenaState->GetSnapshot();
	Snapshot.AliveKnights = 0;
	Snapshot.DownedKnights = 0;
	for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		Snapshot.AliveKnights += Entry.Knight->GetLifeComponent()->IsAlive() ? 1 : 0;
		Snapshot.DownedKnights += Entry.Knight->GetLifeComponent()->IsDowned() ? 1 : 0;
	}
	Snapshot.AliveEnemies = 0;
	for (AKnightArenaEnemy* Enemy : Enemies)
	{
		Snapshot.AliveEnemies += IsValid(Enemy) && Enemy->HasActorBegunPlay()
			&& !Enemy->GetLifeComponent()->IsDead() ? 1 : 0;
	}
	if (Snapshot.Stage == EKnightArenaStage::Wave || Snapshot.Stage == EKnightArenaStage::Boss)
	{
		if (Snapshot.AliveKnights == 0 || bUnexpectedEnemyLoss)
		{
			Snapshot.Stage = EKnightArenaStage::Defeat;
			Snapshot.Failure = bUnexpectedEnemyLoss
				? EKnightArenaFailure::EncounterUnavailable : EKnightArenaFailure::PartyUnavailable;
			for (AKnightArenaEnemy* Enemy : Enemies)
			{
				if (IsValid(Enemy))
				{
					if (AAIController* Controller = Cast<AAIController>(Enemy->GetController()))
					{
						Controller->StopMovement();
						if (UBrainComponent* Brain = Controller->GetBrainComponent())
						{
							Brain->StopLogic(TEXT("Party defeated"));
						}
					}
				}
			}
		}
		else if (Snapshot.AliveEnemies == 0 && Snapshot.Stage == EKnightArenaStage::Wave && ShouldAdvanceWaveToBoss())
		{
			const FKnightResult Spawn = SpawnEncounter(EKnightArenaStage::Boss);
			if (Spawn.IsSuccess())
			{
				return;
			}
			Snapshot.Stage = EKnightArenaStage::Defeat;
			Snapshot.Failure = EKnightArenaFailure::EncounterUnavailable;
		}
		else if (Snapshot.AliveEnemies == 0)
		{
			Snapshot.Stage = EKnightArenaStage::Victory;
		}
	}
	ArenaState->CommitSnapshot(Snapshot);
}

bool AKnightPartyArenaGameMode::ShouldAdvanceWaveToBoss() const
{
	return true;
}

FKnightResult AKnightPartyArenaGameMode::StartFixtureEncounter(const EKnightArenaStage Stage, const bool bElite)
{
	if (!HasAuthority() || ArenaState == nullptr || ArenaState->GetSnapshot().AliveEnemies != 0)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("FixtureEncounter"), FText::GetEmpty());
	}
	bFixtureElite = bElite;
	return SpawnEncounter(Stage);
}

void AKnightPartyArenaGameMode::HandleBossPhaseChanged(const FKnightBossPhaseSnapshot& Phase)
{
	if (!bResetting && !bEndingPlay && ArenaState != nullptr)
	{
		FKnightPartyArenaSnapshot Snapshot = ArenaState->GetSnapshot();
		Snapshot.BossPhase = Phase.PhaseIndex;
		ArenaState->CommitSnapshot(Snapshot);
	}
}

FKnightResult AKnightPartyArenaGameMode::TryRestartDrill(APlayerController* Player)
{
	AKnightPartyState* Party = GetPartyState();
	if (!HasAuthority() || !IsValid(Player) || Player->GetWorld() != GetWorld())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("ArenaRetry"), FText::GetEmpty());
	}
	if (!Player->IsLocalController() || Party == nullptr || !Party->IsCanonicalRosterReady()
		|| Party->GetControlledSlot(Player) == INDEX_NONE || ArenaState == nullptr)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("ArenaRetry"),
			NSLOCTEXT("KnightArena", "RetryHost", "Only the host controlling a canonical Knight may retry."));
	}
	for (const FKnightRosterEntry& Entry : Party->GetRoster())
	{
		if (Entry.Knight->GetKnightAbilitySystem()->IsMutationFenced())
		{
			return FKnightResult::Failure(EKnightError::Conflict, TEXT("ArenaRetry"),
				NSLOCTEXT("KnightArena", "RetryTransactionActive", "Wait for the Knight's transaction to finish."));
		}
	}
	bResetting = true;
	ON_SCOPE_EXIT
	{
		bResetting = false;
	};
	GetWorldTimerManager().ClearTimer(RefreshTimer);
	ClearEncounter();
	for (const FKnightRosterEntry& Entry : Party->GetRoster())
	{
		AKnightCharacter* Knight = Entry.Knight;
		if (AKnightArenaCompanionController* Companion = Cast<AKnightArenaCompanionController>(Knight->GetController()))
		{
			Companion->StopMovement();
			if (UBrainComponent* Brain = Companion->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Arena checkpoint reset"));
			}
			Companion->GetAIContext()->ClearControlledPawn();
		}
		if (UKnightTraversalComponent* Traversal = Knight->FindComponentByClass<UKnightTraversalComponent>();
			Traversal != nullptr && Traversal->HasActiveTraversal())
		{
			const FGuid Handle = Traversal->GetSnapshot().Handle;
			Traversal->EndTraversal(Handle, true);
		}
		if (UKnightFinisherComponent* Finisher = Knight->FindComponentByClass<UKnightFinisherComponent>();
			Finisher != nullptr && Finisher->HasActiveFinisher())
		{
			const FGuid Reservation = Finisher->GetReservation();
			Finisher->EndFinisher(Reservation, false);
		}
		Knight->ResetControlInput();
		Knight->GetCombatComponent()->ClearTransientActions();
		const FKnightResult LifeReset = Knight->GetLifeComponent()->ResetForCheckpointWithDeferredControllerRestart();
		if (!LifeReset.IsSuccess())
		{
			return LifeReset;
		}
		const FKnightResult CombatReset = Knight->GetCombatComponent()->ResetForCheckpoint();
		if (!CombatReset.IsSuccess())
		{
			return CombatReset;
		}
	}
	const FKnightResult Reset = Super::TryRestartDrill(Player);
	if (!Reset.IsSuccess())
	{
		return Reset;
	}
	for (const FKnightRosterEntry& Entry : Party->GetRoster())
	{
		if (AKnightArenaCompanionController* Companion = Cast<AKnightArenaCompanionController>(Entry.Knight->GetController()))
		{
			const FKnightResult Refresh = Companion->RefreshArenaConfiguration();
			if (!Refresh.IsSuccess())
			{
				return Refresh;
			}
			const FKnightResult Follow = Companion->TrySetCommand(EKnightCompanionCommand::Follow);
			if (!Follow.IsSuccess())
			{
				return Follow;
			}
		}
	}
	ArenaState->CommitSnapshot(FKnightPartyArenaSnapshot());
	return FKnightResult::Success();
}

void AKnightPartyArenaGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	GetWorldTimerManager().ClearTimer(RefreshTimer);
	for (const FKnightArenaLifeBinding& Binding : PartyBindings)
	{
		if (Binding.Life.IsValid())
		{
			Binding.Life->OnLifeStateChanged.Remove(Binding.Handle);
		}
	}
	PartyBindings.Reset();
	// World teardown destroys encounter actors; only explicit retry/progression owns their destruction.
	for (const FKnightArenaLifeBinding& Binding : EnemyBindings)
	{
		if (Binding.Life.IsValid())
		{
			Binding.Life->OnLifeStateChanged.Remove(Binding.Handle);
		}
	}
	EnemyBindings.Reset();
	for (AKnightArenaEnemy* Enemy : Enemies)
	{
		if (IsValid(Enemy))
		{
			Enemy->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleEncounterEnemyEndPlay);
			Enemy->GetBossPhaseComponent()->OnPhaseChanged.Remove(BossPhaseHandle);
		}
	}
	Enemies.Reset();
	BossPhaseHandle.Reset();
	Super::EndPlay(EndPlayReason);
}
