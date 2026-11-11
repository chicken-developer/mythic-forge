#include "SkillLab/GolemSkillLabGameMode.h"

#include "Boss/GolemBossPhaseComponent.h"
#include "AbilityYard/GolemAbilityDummy.h"
#include "Attributes/GolemAttributeSet.h"
#include "Components/TextRenderComponent.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Loot/GolemItemPickup.h"
#include "Party/GolemPartyState.h"
#include "PartyArena/GolemArenaEnemy.h"
#include "Progression/GolemRPGComponent.h"
#include "SkillLab/GolemBossArmorComponent.h"
#include "SkillLab/GolemFrozenSurface.h"
#include "SkillLab/GolemRunestone.h"
#include "SkillLab/GolemRunestonePickup.h"
#include "SkillLab/GolemSkillLabPlayerController.h"
#include "SkillLab/GolemSkillLabHUD.h"
#include "Skills/GolemSkillTypes.h"
#include "Skills/GolemSkillRuntimeComponent.h"
#include "TimerManager.h"

AGolemSkillLabGameMode::AGolemSkillLabGameMode()
{
	PlayerControllerClass = AGolemSkillLabPlayerController::StaticClass();
	HUDClass = AGolemSkillLabHUD::StaticClass();
}

FGolemResult AGolemSkillLabGameMode::ConfigureRPGDomains(UGolemInventoryComponent* Inventory,
	UGolemRPGComponent* RPG)
{
	TMap<FPrimaryAssetId, FGolemSkillBuild> Builds;
	for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		FGolemSkillBuild Build;
		Build.SpecId = Entry.HeroDefinitionId.PrimaryAssetName == TEXT("Training_Red") ? TEXT("Uranus")
			: (Entry.HeroDefinitionId.PrimaryAssetName == TEXT("Training_Blue") ? TEXT("Pluto") : TEXT("Terra"));
		Builds.Add(Entry.HeroDefinitionId, Build);
	}
	FGolemResult Result = RPG->ConfigureSkillLabBuilds(Builds);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FGolemInventoryRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.ExpectedRevision = Inventory->GetView().Revision;
	// Actual definition-backed items, seeded before the coordinator's initial snapshot/journal boundary.
	for (const FName Name : { FName(TEXT("Aftershock")), FName(TEXT("Convergence")),
		FName(TEXT("Propagation")), FName(TEXT("Mobile")) })
	{
		FGolemInventoryMutation Mutation;
		Mutation.Item.InstanceId = FGuid::NewGuid();
		Mutation.Item.DefinitionId = FPrimaryAssetId(TEXT("KnightItem"), Name);
		Mutation.Item.Count = 1;
		Request.Mutations.Add(Mutation);
	}
	return Inventory->TryApplyTransaction(Request).Result;
}

void AGolemSkillLabGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (!GetRPGInitializationResult().IsSuccess())
	{
		return;
	}
	EncounterId = FGuid::NewGuid();
	// Placed actors initialize their GAS in BeginPlay after GameMode's BeginPlay.
	DummyReadinessTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::ResetTrainingDummy);
	for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		if (UGolemSkillRuntimeComponent* Runtime = Entry.Golem->FindComponentByClass<UGolemSkillRuntimeComponent>())
		{
			Runtime->SetEncounterId(EncounterId);
		}
	}
	for (TActorIterator<AGolemRunestone> It(GetWorld()); It; ++It)
	{
		It->ResetFixture(EncounterId);
		Runestones.Add(*It);
	}
	for (TActorIterator<AGolemFrozenSurface> It(GetWorld()); It; ++It)
	{
		FrozenSurface = *It;
	}
	ArenaHandle = GetArenaState()->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleLabArenaChanged);
	HandleLabArenaChanged(GetArenaState()->GetSnapshot());
}

void AGolemSkillLabGameMode::ResetTrainingDummy()
{
	for (TActorIterator<AGolemAbilityDummy> It(GetWorld()); It; ++It)
	{
		It->GetLifeComponent()->ResetForCheckpoint();
		It->GetGolemAbilitySystem()->SetNumericAttributeBase(UGolemAttributeSet::GetMaxHealthAttribute(), 10000.0f);
		It->GetGolemAbilitySystem()->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), 10000.0f);
		It->GetGolemAbilitySystem()->SetNumericAttributeBase(UGolemAttributeSet::GetDefenseAttribute(), 100.0f);
		if (UTextRenderComponent* Label = It->FindComponentByClass<UTextRenderComponent>())
		{
			Label->SetText(NSLOCTEXT("KnightSkillLab", "DummyLabel", "SKILL DUMMY: Z/X/C/V, 100 Armor; R retries after victory/defeat"));
		}
	}
}

void AGolemSkillLabGameMode::HandleLabArenaChanged(const FGolemPartyArenaSnapshot& Snapshot)
{
	if (Snapshot.Stage != EGolemArenaStage::Boss)
	{
		return;
	}
	for (AGolemArenaEnemy* Enemy : GetEncounterEnemies())
	{
		UGolemBossArmorComponent* Armor = Enemy->FindComponentByClass<UGolemBossArmorComponent>();
		if (Armor != nullptr && Armor->GetArmorZone() == nullptr && Enemy->GetBossPhaseComponent()->GetDefinitionId().IsValid())
		{
			Armor->EnableArmorZone(EncounterId);
		}
	}
}

FGolemResult AGolemSkillLabGameMode::TryInteract(APlayerController* Player)
{
	AGolemCharacter* Golem = IsValid(Player) ? Cast<AGolemCharacter>(Player->GetPawn()) : nullptr;
	if (!HasAuthority() || Golem == nullptr || GetPartyState()->GetControlledSlot(Player) == INDEX_NONE
		|| !Golem->GetLifeComponent()->IsAlive())
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("SkillLabInteract"), FText::GetEmpty());
	}
	AGolemRunestonePickup* Pickup = nullptr;
	AGolemRunestone* Stone = nullptr;
	double BestDistance = FMath::Square(200.0);
	for (TActorIterator<AGolemRunestonePickup> It(GetWorld()); It; ++It)
	{
		const double Distance = FVector::DistSquared(Golem->GetActorLocation(), It->GetActorLocation());
		if (It->GetRemainingCharges() > 0 && Distance <= BestDistance)
		{
			BestDistance = Distance;
			Pickup = *It;
		}
	}
	if (Pickup != nullptr)
	{
		return Pickup->TryCollect(Golem, FGuid::NewGuid());
	}
	for (AGolemRunestone* Candidate : Runestones)
	{
		const double Distance = FVector::DistSquared(Golem->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance <= BestDistance)
		{
			BestDistance = Distance;
			Stone = Candidate;
		}
	}
	if (Stone != nullptr)
	{
		return Stone->TryActivate(Golem, FGuid::NewGuid());
	}
	for (TActorIterator<AGolemItemPickup> It(GetWorld()); It; ++It)
	{
		if (!It->IsClaimed() && FVector::DistSquared(Golem->GetActorLocation(), It->GetActorLocation())
			<= FMath::Square(static_cast<double>(GolemRPG::PickupRange)))
		{
			return Super::TryInteract(Player);
		}
	}
	// Isolated damage fixture at the sanctuary: deliberate hurt command for healer/Runestone testing.
	if (IsInUpgradeSafeZone(Golem))
	{
		return UGolemAbilitySystemComponent::ApplyDamage(Golem, Golem, 150.0f);
	}
	return Super::TryInteract(Player);
}

void AGolemSkillLabGameMode::ResetLabFixtures()
{
	EncounterId = FGuid::NewGuid();
	ResetTrainingDummy();
	for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		if (UGolemSkillRuntimeComponent* Runtime = Entry.Golem->FindComponentByClass<UGolemSkillRuntimeComponent>())
		{
			Runtime->SetEncounterId(EncounterId);
		}
	}
	for (AGolemRunestone* Stone : Runestones)
	{
		Stone->ResetFixture(EncounterId);
	}
	if (FrozenSurface != nullptr)
	{
		FrozenSurface->ResetSurface();
	}
}

FGolemResult AGolemSkillLabGameMode::TryRestartDrill(APlayerController* Player)
{
	const FGolemResult Result = Super::TryRestartDrill(Player);
	if (Result.IsSuccess())
	{
		ResetLabFixtures();
	}
	return Result;
}

FGuid AGolemSkillLabGameMode::GetLabEncounterId() const
{
	return EncounterId;
}

void AGolemSkillLabGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DummyReadinessTimer);
	if (GetArenaState() != nullptr)
	{
		GetArenaState()->OnSnapshotChanged.Remove(ArenaHandle);
	}
	Super::EndPlay(EndPlayReason);
}
