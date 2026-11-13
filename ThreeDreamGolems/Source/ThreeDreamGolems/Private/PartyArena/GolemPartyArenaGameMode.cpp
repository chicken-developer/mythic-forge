#include "PartyArena/GolemPartyArenaGameMode.h"

#include "Assets/GolemAssetManager.h"
#include "Boss/GolemBossPhaseComponent.h"
#include "BrainComponent.h"
#include "Combat/GolemCombatDirectorSubsystem.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Context/GolemAIContextComponent.h"
#include "Controllers/GolemArenaCompanionController.h"
#include "Definitions/GolemAIArchetypeDefinition.h"
#include "Definitions/GolemBossPhaseDefinition.h"
#include "Party/GolemPartyState.h"
#include "PartyArena/GolemArenaCharacter.h"
#include "PartyArena/GolemArenaEnemy.h"
#include "PartyArena/GolemArenaPlayerController.h"
#include "PartyArena/GolemPartyArenaHUD.h"
#include "PartyArena/GolemPartyArenaStartMarker.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemPartyArena, Log, All);

AGolemPartyArenaGameMode::AGolemPartyArenaGameMode()
{
	GolemCharacterClass = AGolemArenaCharacter::StaticClass();
	AIControllerClass = AGolemArenaCompanionController::StaticClass();
	PlayerControllerClass = AGolemArenaPlayerController::StaticClass();
	HUDClass = AGolemPartyArenaHUD::StaticClass();
	ArenaInitializationResult = FGolemResult::Failure(EGolemError::InvalidState, TEXT("PartyArena"),
		NSLOCTEXT("GolemArena", "ArenaPending", "The party arena is not initialized yet."));
}

void AGolemPartyArenaGameMode::InitGameState()
{
	Super::InitGameState();
	ArenaInitializationResult = GetInitializationResult();
	if (!HasAuthority() || !ArenaInitializationResult.IsSuccess())
	{
		return;
	}
	for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		AGolemArenaCharacter* Golem = Cast<AGolemArenaCharacter>(Entry.Golem);
		if (Golem == nullptr)
		{
			ArenaInitializationResult = FGolemResult::Failure(EGolemError::TypeMismatch,
				TEXT("PartyArenaPawn"), FText::GetEmpty());
			return;
		}
		ArenaInitializationResult = Golem->InitializeArenaCombat();
		if (!ArenaInitializationResult.IsSuccess())
		{
			return;
		}
		if (AGolemArenaCompanionController* Companion = Cast<AGolemArenaCompanionController>(Golem->GetController()))
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
	ArenaState = GetWorld()->SpawnActor<AGolemPartyArenaState>(Parameters);
	if (ArenaState == nullptr)
	{
		ArenaInitializationResult = FGolemResult::Failure(EGolemError::InvalidState,
			TEXT("PartyArenaState"), FText::GetEmpty());
	}
}

void AGolemPartyArenaGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (!ArenaInitializationResult.IsSuccess() || ArenaState == nullptr)
	{
		UE_LOG(LogGolemPartyArena, Error, TEXT("Arena initialization failed: %s"),
			*ArenaInitializationResult.Message.ToString());
		return;
	}
	for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		FGolemArenaLifeBinding Binding;
		Binding.Life = Entry.Golem->GetLifeComponent();
		Binding.Handle = Binding.Life->OnLifeStateChanged.AddWeakLambda(this,
			[this](EGolemLifeState Previous, EGolemLifeState Current)
			{
				ScheduleEncounterRefresh();
			});
		PartyBindings.Add(Binding);
	}
	RefreshEncounter();
}

AGolemPartyArenaState* AGolemPartyArenaGameMode::GetArenaState() const
{
	return ArenaState;
}

const FGolemResult& AGolemPartyArenaGameMode::GetArenaInitializationResult() const
{
	return ArenaInitializationResult;
}

const TArray<TObjectPtr<AGolemArenaEnemy>>& AGolemPartyArenaGameMode::GetEncounterEnemies() const
{
	return Enemies;
}

FGolemResult AGolemPartyArenaGameMode::TryInteract(APlayerController* Player)
{
	AGolemPartyState* Party = GetPartyState();
	AGolemCharacter* Golem = IsValid(Player) ? Cast<AGolemCharacter>(Player->GetPawn()) : nullptr;
	if (!HasAuthority() || !IsValid(Player) || Player->GetWorld() != GetWorld())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("StartArena"), FText::GetEmpty());
	}
	if (Party == nullptr || Party->GetControlledSlot(Player) == INDEX_NONE || Golem == nullptr
		|| !Golem->GetLifeComponent()->IsAlive() || !ArenaInitializationResult.IsSuccess() || ArenaState == nullptr)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("StartArena"),
			NSLOCTEXT("GolemArena", "StartOwner", "Control a living canonical Golem to start the arena."));
	}
	if (ArenaState->GetSnapshot().Stage != EGolemArenaStage::AwaitingStart)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("StartArena"),
			NSLOCTEXT("GolemArena", "StartBusy", "The encounter has already started. The host can retry with R."));
	}
	AGolemPartyArenaStartMarker* Marker = nullptr;
	for (TActorIterator<AGolemPartyArenaStartMarker> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		if (Marker != nullptr)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("StartArena"), FText::GetEmpty());
		}
		Marker = *Iterator;
	}
	if (Marker == nullptr || FVector::DistSquared(Golem->GetActorLocation(), Marker->GetActorLocation())
		> FMath::Square(Marker->GetInteractionRange()))
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("StartArena"),
			NSLOCTEXT("GolemArena", "StartRange", "Move to the yellow START marker and press F."));
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GolemArenaStart), false, Golem);
	Query.AddIgnoredActor(Marker);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Golem->GetActorLocation(),
		Marker->GetActorLocation() + FVector(0.0, 0.0, 75.0), ECC_Visibility, Query))
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("StartArena"),
			NSLOCTEXT("GolemArena", "StartOccluded", "The START marker must be visible."));
	}
	return SpawnEncounter(EGolemArenaStage::Wave);
}

TArray<FGolemArenaSpawn> AGolemPartyArenaGameMode::GetEncounterSpawns(const EGolemArenaStage Stage) const
{
	const bool bBoss = Stage == EGolemArenaStage::Boss;
	const FPrimaryAssetId Archetype(TEXT("GolemAIArchetype"), bBoss ? TEXT("ArenaBoss") : TEXT("ArenaEnemy"));
	const FPrimaryAssetId Boss = bBoss ? FPrimaryAssetId(TEXT("GolemBossPhase"), TEXT("ArenaBoss")) : FPrimaryAssetId();
	if (bBoss || bFixtureElite)
	{
		return {{Archetype, Boss, FVector(0.0, bBoss ? 850.0 : 500.0, 100.0)}};
	}
	return {{Archetype, Boss, FVector(-600.0, 250.0, 100.0)}, {Archetype, Boss, FVector(600.0, 250.0, 100.0)}};
}

FGolemResult AGolemPartyArenaGameMode::SpawnEncounter(const EGolemArenaStage Stage)
{
	if (Stage != EGolemArenaStage::Wave && Stage != EGolemArenaStage::Boss)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("ArenaSpawn"), FText::GetEmpty());
	}
	const bool bBoss = Stage == EGolemArenaStage::Boss;
	const TArray<FGolemArenaSpawn> Spawns = GetEncounterSpawns(Stage);
	if (Spawns.IsEmpty() || Spawns.Num() > 32 || bBoss && Spawns.Num() != 1)
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("EncounterSpawns"), FText::GetEmpty());
	}
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	UGolemDefinition* Definition = nullptr;
	if (Manager == nullptr)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("ArenaSpawn"), FText::GetEmpty());
	}
	FGolemResult Result = FGolemResult::Success();
	for (const FGolemArenaSpawn& Spawn : Spawns)
	{
		if (Spawn.Position.ContainsNaN() || Spawn.BossId.IsValid() != bBoss)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("EncounterSpawn"), FText::GetEmpty());
		}
		Result = Manager->ResolveDefinition(Spawn.ArchetypeId, UGolemAIArchetypeDefinition::StaticClass(), Definition);
		if (Result.IsSuccess())
		{
			Result = CastChecked<UGolemAIArchetypeDefinition>(Definition)->ValidateArchetype();
		}
		if (Result.IsSuccess() && bBoss)
		{
			Result = Manager->ResolveDefinition(Spawn.BossId, UGolemBossPhaseDefinition::StaticClass(), Definition);
			if (Result.IsSuccess())
			{
				Result = CastChecked<UGolemBossPhaseDefinition>(Definition)->ValidateBossPhases();
			}
		}
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	ClearEncounter();
	for (const FGolemArenaSpawn& Spawn : Spawns)
	{
		const FTransform Transform(FRotator(0.0, -90.0, 0.0), Spawn.Position);
		AGolemArenaEnemy* Enemy = GetWorld()->SpawnActorDeferred<AGolemArenaEnemy>(
			AGolemArenaEnemy::StaticClass(), Transform, this, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (Enemy == nullptr)
		{
			Result = FGolemResult::Failure(EGolemError::InvalidState, TEXT("ArenaSpawn"), FText::GetEmpty());
			break;
		}
		Enemies.Add(Enemy);
		Result = Enemy->InitializeArenaEnemy(Spawn.ArchetypeId, Spawn.BossId);
		Enemy->FinishSpawning(Transform);
		if (Result.IsSuccess())
		{
			Result = Enemy->GetInitializationResult();
		}
		if (!Result.IsSuccess())
		{
			break;
		}
		FGolemArenaLifeBinding Binding;
		Binding.Life = Enemy->GetLifeComponent();
		Binding.Handle = Binding.Life->OnLifeStateChanged.AddWeakLambda(this,
			[this](EGolemLifeState Previous, EGolemLifeState Current)
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
		UE_LOG(LogGolemPartyArena, Error, TEXT("Actual encounter spawn failed: %s"), *Result.Message.ToString());
		return Result;
	}
	FGolemPartyArenaSnapshot Snapshot = ArenaState->GetSnapshot();
	Snapshot.Stage = Stage;
	Snapshot.AliveEnemies = Enemies.Num();
	Snapshot.BossPhase = bBoss ? Enemies[0]->GetBossPhaseComponent()->GetPhaseIndex() : INDEX_NONE;
	ArenaState->CommitSnapshot(Snapshot);
	UE_LOG(LogGolemPartyArena, Display, TEXT("Arena actual stage%d enemies%d canonical%d"),
		static_cast<int32>(Stage), Enemies.Num(), GetPartyState()->GetRoster().Num());
	return Result;
}

void AGolemPartyArenaGameMode::ClearEncounter()
{
	for (const FGolemArenaLifeBinding& Binding : EnemyBindings)
	{
		if (Binding.Life.IsValid())
		{
			Binding.Life->OnLifeStateChanged.Remove(Binding.Handle);
		}
	}
	EnemyBindings.Reset();
	for (AGolemArenaEnemy* Enemy : Enemies)
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

void AGolemPartyArenaGameMode::HandleEncounterEnemyEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	if (bResetting || bEndingPlay || GetWorld() == nullptr || GetWorld()->bIsTearingDown
		|| Reason == EEndPlayReason::LevelTransition || Reason == EEndPlayReason::EndPlayInEditor
		|| Reason == EEndPlayReason::Quit)
	{
		return;
	}
	AGolemArenaEnemy* Enemy = Cast<AGolemArenaEnemy>(Actor);
	if (Enemy != nullptr && !Enemy->GetLifeComponent()->IsDead())
	{
		bUnexpectedEnemyLoss = true;
		ScheduleEncounterRefresh();
	}
}

void AGolemPartyArenaGameMode::ScheduleEncounterRefresh()
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

void AGolemPartyArenaGameMode::RefreshEncounter()
{
	RefreshTimer.Invalidate();
	if (bResetting || bEndingPlay || ArenaState == nullptr || GetPartyState() == nullptr)
	{
		return;
	}
	FGolemPartyArenaSnapshot Snapshot = ArenaState->GetSnapshot();
	Snapshot.AliveGolems = 0;
	Snapshot.DownedGolems = 0;
	for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		Snapshot.AliveGolems += Entry.Golem->GetLifeComponent()->IsAlive() ? 1 : 0;
		Snapshot.DownedGolems += Entry.Golem->GetLifeComponent()->IsDowned() ? 1 : 0;
	}
	Snapshot.AliveEnemies = 0;
	for (AGolemArenaEnemy* Enemy : Enemies)
	{
		Snapshot.AliveEnemies += IsValid(Enemy) && Enemy->HasActorBegunPlay()
			&& !Enemy->GetLifeComponent()->IsDead() ? 1 : 0;
	}
	if (Snapshot.Stage == EGolemArenaStage::Wave || Snapshot.Stage == EGolemArenaStage::Boss)
	{
		if (Snapshot.AliveGolems == 0 || bUnexpectedEnemyLoss)
		{
			Snapshot.Stage = EGolemArenaStage::Defeat;
			Snapshot.Failure = bUnexpectedEnemyLoss
				? EGolemArenaFailure::EncounterUnavailable : EGolemArenaFailure::PartyUnavailable;
			for (AGolemArenaEnemy* Enemy : Enemies)
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
		else if (Snapshot.AliveEnemies == 0 && Snapshot.Stage == EGolemArenaStage::Wave && ShouldAdvanceWaveToBoss())
		{
			const FGolemResult Spawn = SpawnEncounter(EGolemArenaStage::Boss);
			if (Spawn.IsSuccess())
			{
				return;
			}
			Snapshot.Stage = EGolemArenaStage::Defeat;
			Snapshot.Failure = EGolemArenaFailure::EncounterUnavailable;
		}
		else if (Snapshot.AliveEnemies == 0)
		{
			Snapshot.Stage = EGolemArenaStage::Victory;
		}
	}
	ArenaState->CommitSnapshot(Snapshot);
}

bool AGolemPartyArenaGameMode::ShouldAdvanceWaveToBoss() const
{
	return true;
}

FGolemResult AGolemPartyArenaGameMode::StartFixtureEncounter(const EGolemArenaStage Stage, const bool bElite)
{
	if (!HasAuthority() || ArenaState == nullptr || ArenaState->GetSnapshot().AliveEnemies != 0)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("FixtureEncounter"), FText::GetEmpty());
	}
	bFixtureElite = bElite;
	return SpawnEncounter(Stage);
}

void AGolemPartyArenaGameMode::HandleBossPhaseChanged(const FGolemBossPhaseSnapshot& Phase)
{
	if (!bResetting && !bEndingPlay && ArenaState != nullptr)
	{
		FGolemPartyArenaSnapshot Snapshot = ArenaState->GetSnapshot();
		Snapshot.BossPhase = Phase.PhaseIndex;
		ArenaState->CommitSnapshot(Snapshot);
	}
}

FGolemResult AGolemPartyArenaGameMode::TryRestartDrill(APlayerController* Player)
{
	AGolemPartyState* Party = GetPartyState();
	if (!HasAuthority() || !IsValid(Player) || Player->GetWorld() != GetWorld())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("ArenaRetry"), FText::GetEmpty());
	}
	if (!Player->IsLocalController() || Party == nullptr || !Party->IsCanonicalRosterReady()
		|| Party->GetControlledSlot(Player) == INDEX_NONE || ArenaState == nullptr)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("ArenaRetry"),
			NSLOCTEXT("GolemArena", "RetryHost", "Only the host controlling a canonical Golem may retry."));
	}
	for (const FGolemRosterEntry& Entry : Party->GetRoster())
	{
	}
	bResetting = true;
	ON_SCOPE_EXIT
	{
		bResetting = false;
	};
	GetWorldTimerManager().ClearTimer(RefreshTimer);
	ClearEncounter();
	for (const FGolemRosterEntry& Entry : Party->GetRoster())
	{
		AGolemCharacter* Golem = Entry.Golem;
		if (AGolemArenaCompanionController* Companion = Cast<AGolemArenaCompanionController>(Golem->GetController()))
		{
			Companion->StopMovement();
			if (UBrainComponent* Brain = Companion->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Arena checkpoint reset"));
			}
			Companion->GetAIContext()->ClearControlledPawn();
		}
		Golem->ResetControlInput();
		Golem->GetCombatComponent()->ClearTransientActions();
		const FGolemResult LifeReset = Golem->GetLifeComponent()->ResetForCheckpointWithDeferredControllerRestart();
		if (!LifeReset.IsSuccess())
		{
			return LifeReset;
		}
		const FGolemResult CombatReset = Golem->GetCombatComponent()->ResetForCheckpoint();
		if (!CombatReset.IsSuccess())
		{
			return CombatReset;
		}
	}
	const FGolemResult Reset = Super::TryRestartDrill(Player);
	if (!Reset.IsSuccess())
	{
		return Reset;
	}
	for (const FGolemRosterEntry& Entry : Party->GetRoster())
	{
		if (AGolemArenaCompanionController* Companion = Cast<AGolemArenaCompanionController>(Entry.Golem->GetController()))
		{
			const FGolemResult Refresh = Companion->RefreshArenaConfiguration();
			if (!Refresh.IsSuccess())
			{
				return Refresh;
			}
			const FGolemResult Follow = Companion->TrySetCommand(EGolemCompanionCommand::Follow);
			if (!Follow.IsSuccess())
			{
				return Follow;
			}
		}
	}
	ArenaState->CommitSnapshot(FGolemPartyArenaSnapshot());
	return FGolemResult::Success();
}

void AGolemPartyArenaGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	GetWorldTimerManager().ClearTimer(RefreshTimer);
	for (const FGolemArenaLifeBinding& Binding : PartyBindings)
	{
		if (Binding.Life.IsValid())
		{
			Binding.Life->OnLifeStateChanged.Remove(Binding.Handle);
		}
	}
	PartyBindings.Reset();
	// World teardown destroys encounter actors; only explicit retry/progression owns their destruction.
	for (const FGolemArenaLifeBinding& Binding : EnemyBindings)
	{
		if (Binding.Life.IsValid())
		{
			Binding.Life->OnLifeStateChanged.Remove(Binding.Handle);
		}
	}
	EnemyBindings.Reset();
	for (AGolemArenaEnemy* Enemy : Enemies)
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
