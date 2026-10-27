#include "Campaign/KnightEncounterQualityGameMode.h"

#include "Assets/KnightAssetManager.h"
#include "Boss/KnightBossPhaseComponent.h"
#include "Campaign/KnightEncounterQualityDefinition.h"
#include "Character/KnightCharacter.h"
#include "Combat/KnightCombatDirectorSubsystem.h"
#include "Components/KnightLifeComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Party/KnightPartyState.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "SkillLab/KnightBossArmorComponent.h"
#include "SkillLab/KnightFrozenSurface.h"
#include "SkillLab/KnightRunestone.h"
#include "SkillLab/KnightRunestonePickup.h"
#include "Story/KnightStoryState.h"

void AKnightEncounterQualityGameMode::BeginPlay()
{
	Super::BeginPlay();
	UKnightDefinition* Resolved = nullptr;
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	if (GetStoryState() != nullptr && Manager != nullptr && Manager->ResolveDefinition(GetStoryState()->GetWorldDefinitionId(),
		UKnightStoryWorldDefinition::StaticClass(), Resolved).IsSuccess())
	{
		const FPrimaryAssetId EncounterId = Cast<UKnightStoryWorldDefinition>(Resolved)->EncounterQualityDefinitionId;
		if (Manager->ResolveDefinition(EncounterId, UKnightEncounterQualityDefinition::StaticClass(), Resolved).IsSuccess())
		{
			Encounter = Cast<UKnightEncounterQualityDefinition>(Resolved);
		}
	}
	if (Encounter == nullptr || !Encounter->ValidateEncounter().IsSuccess())
	{
		return;
	}
	DifficultyId = GetDifficulty();
	GetWorld()->GetSubsystem<UKnightCombatDirectorSubsystem>()->TrySetTeamBudgets(2,
		DifficultyId == TEXT("Story") ? Encounter->StoryAttackBudget : Encounter->StandardAttackBudget, 1);
	QualityArenaHandle = GetArenaState()->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleQualityArenaChanged);
}

FName AKnightEncounterQualityGameMode::GetDifficulty() const
{
	if (GetStoryState() != nullptr)
	{
		const TArray<FKnightWorldFact>& Facts = GetStoryState()->WorldState->GetSnapshot().Facts;
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

FKnightResult AKnightEncounterQualityGameMode::TrySetDifficulty(APlayerController* Player, const FName Difficulty)
{
	if (!HasAuthority() || !IsValid(Player) || !Player->IsLocalController()
		|| GetPartyState()->GetControlledSlot(Player) == INDEX_NONE || Encounter == nullptr
		|| (Difficulty != TEXT("Story") && Difficulty != TEXT("Standard")))
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("DifficultyCaller"), FText::GetEmpty());
	}
	FKnightCampaignCheckpoint Boundary;
	FKnightResult Result = CaptureCheckpoint(Boundary);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	UKnightCombatDirectorSubsystem* Director = GetWorld()->GetSubsystem<UKnightCombatDirectorSubsystem>();
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

TArray<FKnightArenaSpawn> AKnightEncounterQualityGameMode::GetEncounterSpawns(const EKnightArenaStage Stage) const
{
	if (Encounter == nullptr)
	{
		return {};
	}
	if (Stage == EKnightArenaStage::Boss)
	{
		return {{Encounter->BossArchetypeId, Encounter->BossPhaseId, FVector(0.0, 850.0, 100.0)}};
	}
	TArray<FKnightArenaSpawn> Spawns;
	for (const FKnightEncounterPressure& Pressure : Encounter->Wave)
	{
		Spawns.Add({Pressure.ArchetypeId, FPrimaryAssetId(), Pressure.Position});
	}
	return Spawns;
}

bool AKnightEncounterQualityGameMode::ShouldAdvanceWaveToBoss() const
{
	return true;
}

void AKnightEncounterQualityGameMode::HandleQualityArenaChanged(const FKnightPartyArenaSnapshot& Snapshot)
{
	if (Snapshot.Stage == EKnightArenaStage::Boss)
	{
		for (AKnightArenaEnemy* Enemy : GetEncounterEnemies())
		{
			if (UKnightBossArmorComponent* Armor = Enemy->FindComponentByClass<UKnightBossArmorComponent>();
				Armor != nullptr && Armor->GetArmorZone() == nullptr)
			{
				Armor->EnableArmorZone(GetStoryState()->WorldState->GetSnapshot().EncounterRunId);
			}
		}
	}
}

FKnightResult AKnightEncounterQualityGameMode::TryInteract(APlayerController* Player)
{
	AKnightCharacter* Knight = IsValid(Player) ? Cast<AKnightCharacter>(Player->GetPawn()) : nullptr;
	if (!HasAuthority() || Knight == nullptr || Knight->GetController() != Player
		|| GetPartyState()->GetControlledSlot(Player) == INDEX_NONE || !Knight->GetLifeComponent()->IsAlive())
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("EncounterInteract"), FText::GetEmpty());
	}
	for (TActorIterator<AKnightRunestonePickup> It(GetWorld()); It; ++It)
	{
		if (It->GetRemainingCharges() > 0 && FVector::DistSquared(Knight->GetActorLocation(), It->GetActorLocation()) <= FMath::Square(200.0))
		{
			return It->TryCollect(Knight, FGuid::NewGuid());
		}
	}
	for (TActorIterator<AKnightRunestone> It(GetWorld()); It; ++It)
	{
		if (FVector::DistSquared(Knight->GetActorLocation(), It->GetActorLocation()) <= FMath::Square(200.0))
		{
			return It->TryActivate(Knight, FGuid::NewGuid());
		}
	}
	return Super::TryInteract(Player);
}

FKnightResult AKnightEncounterQualityGameMode::TryRestartDrill(APlayerController* Player)
{
	const FKnightResult Result = Super::TryRestartDrill(Player);
	if (Result.IsSuccess())
	{
		// Retire channel timers and spawned pickups immediately, before another encounter begins.
		for (TActorIterator<AKnightRunestone> It(GetWorld()); It; ++It)
		{
			It->ResetFixture(FGuid());
		}
		for (TActorIterator<AKnightFrozenSurface> It(GetWorld()); It; ++It)
		{
			It->ResetSurface();
		}
	}
	return Result;
}

void AKnightEncounterQualityGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetArenaState() != nullptr)
	{
		GetArenaState()->OnSnapshotChanged.Remove(QualityArenaHandle);
	}
	Super::EndPlay(Reason);
}
