#include "Campaign/GolemEncounterQualityGameMode.h"

#include "Assets/GolemAssetManager.h"
#include "Boss/GolemBossPhaseComponent.h"
#include "Campaign/GolemEncounterQualityDefinition.h"
#include "Character/GolemCharacter.h"
#include "Combat/GolemCombatDirectorSubsystem.h"
#include "Components/GolemLifeComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Party/GolemPartyState.h"
#include "PartyArena/GolemArenaEnemy.h"
#include "SkillLab/GolemBossArmorComponent.h"
#include "SkillLab/GolemFrozenSurface.h"
#include "SkillLab/GolemRunestone.h"
#include "SkillLab/GolemRunestonePickup.h"
#include "Story/GolemStoryState.h"

void AGolemEncounterQualityGameMode::BeginPlay()
{
	Super::BeginPlay();
	UGolemDefinition* Resolved = nullptr;
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	if (GetStoryState() != nullptr && Manager != nullptr && Manager->ResolveDefinition(GetStoryState()->GetWorldDefinitionId(),
		UGolemStoryWorldDefinition::StaticClass(), Resolved).IsSuccess())
	{
		const FPrimaryAssetId EncounterId = Cast<UGolemStoryWorldDefinition>(Resolved)->EncounterQualityDefinitionId;
		if (Manager->ResolveDefinition(EncounterId, UGolemEncounterQualityDefinition::StaticClass(), Resolved).IsSuccess())
		{
			Encounter = Cast<UGolemEncounterQualityDefinition>(Resolved);
		}
	}
	if (Encounter == nullptr || !Encounter->ValidateEncounter().IsSuccess())
	{
		return;
	}
	DifficultyId = GetDifficulty();
	GetWorld()->GetSubsystem<UGolemCombatDirectorSubsystem>()->TrySetTeamBudgets(2,
		DifficultyId == TEXT("Story") ? Encounter->StoryAttackBudget : Encounter->StandardAttackBudget, 1);
	QualityArenaHandle = GetArenaState()->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleQualityArenaChanged);
}

FName AGolemEncounterQualityGameMode::GetDifficulty() const
{
	if (GetStoryState() != nullptr)
	{
		const TArray<FGolemWorldFact>& Facts = GetStoryState()->WorldState->GetSnapshot().Facts;
		for (int32 Index = Facts.Num() - 1; Index >= 0; --Index)
		{
			if (Encounter != nullptr && Facts[Index].EventName == TEXT("DifficultyChosen")
				&& Facts[Index].SubjectId == Encounter->GetPrimaryAssetId().PrimaryAssetName)
			{
				return Facts[Index].ValueId;
			}
		}
	}
	return DifficultyId;
}

FGolemResult AGolemEncounterQualityGameMode::TrySetDifficulty(APlayerController* Player, const FName Difficulty)
{
	if (!HasAuthority() || !IsValid(Player) || !Player->IsLocalController()
		|| GetPartyState()->GetControlledSlot(Player) == INDEX_NONE || Encounter == nullptr
		|| (Difficulty != TEXT("Story") && Difficulty != TEXT("Standard")))
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("DifficultyCaller"), FText::GetEmpty());
	}
	FGolemCampaignCheckpoint Boundary;
	FGolemResult Result = CaptureCheckpoint(Boundary);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UGolemCombatDirectorSubsystem* Director = GetWorld()->GetSubsystem<UGolemCombatDirectorSubsystem>();
	const int32 PreviousBudget = Director->GetHostileBudget();
	Result = Director->TrySetTeamBudgets(2, Difficulty == TEXT("Story") ? Encounter->StoryAttackBudget : Encounter->StandardAttackBudget, 1);
	if (Result.IsSuccess())
	{
		Result = GetStoryState()->WorldState->CommitFact(TEXT("DifficultyChosen"), Encounter->GetPrimaryAssetId().PrimaryAssetName, Difficulty);
		if (Result.IsSuccess())
		{
			DifficultyId = Difficulty;
		}
		else
		{
			Director->TrySetTeamBudgets(2, PreviousBudget, 1);
		}
	}
	return Result;
}

TArray<FGolemArenaSpawn> AGolemEncounterQualityGameMode::GetEncounterSpawns(const EGolemArenaStage Stage) const
{
	if (Encounter == nullptr)
	{
		return {};
	}
	if (Stage == EGolemArenaStage::Boss)
	{
		return {{Encounter->BossArchetypeId, Encounter->BossPhaseId, FVector(0.0, 850.0, 100.0)}};
	}
	TArray<FGolemArenaSpawn> Spawns;
	for (const FGolemEncounterPressure& Pressure : Encounter->Wave)
	{
		Spawns.Add({Pressure.ArchetypeId, FPrimaryAssetId(), Pressure.Position});
	}
	return Spawns;
}

bool AGolemEncounterQualityGameMode::ShouldAdvanceWaveToBoss() const
{
	return true;
}

void AGolemEncounterQualityGameMode::HandleQualityArenaChanged(const FGolemPartyArenaSnapshot& Snapshot)
{
	if (Snapshot.Stage == EGolemArenaStage::Boss)
	{
		for (AGolemArenaEnemy* Enemy : GetEncounterEnemies())
		{
			if (UGolemBossArmorComponent* Armor = Enemy->FindComponentByClass<UGolemBossArmorComponent>();
				Armor != nullptr && Armor->GetArmorZone() == nullptr)
			{
				Armor->EnableArmorZone(GetStoryState()->WorldState->GetSnapshot().EncounterRunId);
			}
		}
	}
}

FGolemResult AGolemEncounterQualityGameMode::TryInteract(APlayerController* Player)
{
	AGolemCharacter* Golem = IsValid(Player) ? Cast<AGolemCharacter>(Player->GetPawn()) : nullptr;
	if (!HasAuthority() || Golem == nullptr || Golem->GetController() != Player
		|| GetPartyState()->GetControlledSlot(Player) == INDEX_NONE || !Golem->GetLifeComponent()->IsAlive())
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("EncounterInteract"), FText::GetEmpty());
	}
	for (TActorIterator<AGolemRunestonePickup> It(GetWorld()); It; ++It)
	{
		if (It->GetRemainingCharges() > 0 && FVector::DistSquared(Golem->GetActorLocation(), It->GetActorLocation()) <= FMath::Square(200.0))
		{
			return It->TryCollect(Golem, FGuid::NewGuid());
		}
	}
	for (TActorIterator<AGolemRunestone> It(GetWorld()); It; ++It)
	{
		if (FVector::DistSquared(Golem->GetActorLocation(), It->GetActorLocation()) <= FMath::Square(200.0))
		{
			return It->TryActivate(Golem, FGuid::NewGuid());
		}
	}
	return Super::TryInteract(Player);
}

FGolemResult AGolemEncounterQualityGameMode::TryRestartDrill(APlayerController* Player)
{
	const FGolemResult Result = Super::TryRestartDrill(Player);
	if (Result.IsSuccess())
	{
		// Retire channel timers and spawned pickups immediately, before another encounter begins.
		for (TActorIterator<AGolemRunestone> It(GetWorld()); It; ++It)
		{
			It->ResetFixture(FGuid());
		}
		for (TActorIterator<AGolemFrozenSurface> It(GetWorld()); It; ++It)
		{
			It->ResetSurface();
		}
	}
	return Result;
}

void AGolemEncounterQualityGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetArenaState() != nullptr)
	{
		GetArenaState()->OnSnapshotChanged.Remove(QualityArenaHandle);
	}
	Super::EndPlay(Reason);
}
