#include "SkillLab/KnightSkillLabGameMode.h"

#include "Boss/KnightBossPhaseComponent.h"
#include "AbilityYard/KnightAbilityDummy.h"
#include "Attributes/KnightAttributeSet.h"
#include "Components/TextRenderComponent.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Loot/KnightItemPickup.h"
#include "Party/KnightPartyState.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "Progression/KnightRPGComponent.h"
#include "SkillLab/KnightBossArmorComponent.h"
#include "SkillLab/KnightFrozenSurface.h"
#include "SkillLab/KnightRunestone.h"
#include "SkillLab/KnightRunestonePickup.h"
#include "SkillLab/KnightSkillLabPlayerController.h"
#include "SkillLab/KnightSkillLabHUD.h"
#include "Skills/KnightSkillTypes.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "TimerManager.h"

AKnightSkillLabGameMode::AKnightSkillLabGameMode()
{
	PlayerControllerClass = AKnightSkillLabPlayerController::StaticClass();
	HUDClass = AKnightSkillLabHUD::StaticClass();
}

FKnightResult AKnightSkillLabGameMode::ConfigureRPGDomains(UKnightInventoryComponent* Inventory,
	UKnightRPGComponent* RPG)
{
	TMap<FPrimaryAssetId, FKnightSkillBuild> Builds;
	for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		FKnightSkillBuild Build;
		Build.SpecId = Entry.HeroDefinitionId.PrimaryAssetName == TEXT("Training_Red") ? TEXT("Uranus")
			: (Entry.HeroDefinitionId.PrimaryAssetName == TEXT("Training_Blue") ? TEXT("Pluto") : TEXT("Terra"));
		Builds.Add(Entry.HeroDefinitionId, Build);
	}
	FKnightResult Result = RPG->ConfigureSkillLabBuilds(Builds);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FKnightInventoryRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.ExpectedRevision = Inventory->GetView().Revision;
	// Actual definition-backed items, seeded before the coordinator's initial snapshot/journal boundary.
	for (const FName Name : { FName(TEXT("Aftershock")), FName(TEXT("Convergence")),
		FName(TEXT("Propagation")), FName(TEXT("Mobile")) })
	{
		FKnightInventoryMutation Mutation;
		Mutation.Item.InstanceId = FGuid::NewGuid();
		Mutation.Item.DefinitionId = FPrimaryAssetId(TEXT("KnightItem"), Name);
		Mutation.Item.Count = 1;
		Request.Mutations.Add(Mutation);
	}
	return Inventory->TryApplyTransaction(Request).Result;
}

void AKnightSkillLabGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (!GetRPGInitializationResult().IsSuccess())
	{
		return;
	}
	EncounterId = FGuid::NewGuid();
	// Placed actors initialize their GAS in BeginPlay after GameMode's BeginPlay.
	DummyReadinessTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::ResetTrainingDummy);
	for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		if (UKnightSkillRuntimeComponent* Runtime = Entry.Knight->FindComponentByClass<UKnightSkillRuntimeComponent>())
		{
			Runtime->SetEncounterId(EncounterId);
		}
	}
	for (TActorIterator<AKnightRunestone> It(GetWorld()); It; ++It)
	{
		It->ResetFixture(EncounterId);
		Runestones.Add(*It);
	}
	for (TActorIterator<AKnightFrozenSurface> It(GetWorld()); It; ++It)
	{
		FrozenSurface = *It;
	}
	ArenaHandle = GetArenaState()->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleLabArenaChanged);
	HandleLabArenaChanged(GetArenaState()->GetSnapshot());
}

void AKnightSkillLabGameMode::ResetTrainingDummy()
{
	for (TActorIterator<AKnightAbilityDummy> It(GetWorld()); It; ++It)
	{
		It->GetLifeComponent()->ResetForCheckpoint();
		It->GetKnightAbilitySystem()->SetNumericAttributeBase(UKnightAttributeSet::GetMaxHealthAttribute(), 10000.0f);
		It->GetKnightAbilitySystem()->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), 10000.0f);
		It->GetKnightAbilitySystem()->SetNumericAttributeBase(UKnightAttributeSet::GetDefenseAttribute(), 100.0f);
		if (UTextRenderComponent* Label = It->FindComponentByClass<UTextRenderComponent>())
		{
			Label->SetText(NSLOCTEXT("KnightSkillLab", "DummyLabel", "SKILL DUMMY: Z/X/C/V, 100 Armor; R retries after victory/defeat"));
		}
	}
}

void AKnightSkillLabGameMode::HandleLabArenaChanged(const FKnightPartyArenaSnapshot& Snapshot)
{
	if (Snapshot.Stage != EKnightArenaStage::Boss)
	{
		return;
	}
	for (AKnightArenaEnemy* Enemy : GetEncounterEnemies())
	{
		UKnightBossArmorComponent* Armor = Enemy->FindComponentByClass<UKnightBossArmorComponent>();
		if (Armor != nullptr && Armor->GetArmorZone() == nullptr && Enemy->GetBossPhaseComponent()->GetDefinitionId().IsValid())
		{
			Armor->EnableArmorZone(EncounterId);
		}
	}
}

FKnightResult AKnightSkillLabGameMode::TryInteract(APlayerController* Player)
{
	AKnightCharacter* Knight = IsValid(Player) ? Cast<AKnightCharacter>(Player->GetPawn()) : nullptr;
	if (!HasAuthority() || Knight == nullptr || GetPartyState()->GetControlledSlot(Player) == INDEX_NONE
		|| !Knight->GetLifeComponent()->IsAlive())
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("SkillLabInteract"), FText::GetEmpty());
	}
	AKnightRunestonePickup* Pickup = nullptr;
	AKnightRunestone* Stone = nullptr;
	double BestDistance = FMath::Square(200.0);
	for (TActorIterator<AKnightRunestonePickup> It(GetWorld()); It; ++It)
	{
		const double Distance = FVector::DistSquared(Knight->GetActorLocation(), It->GetActorLocation());
		if (It->GetRemainingCharges() > 0 && Distance <= BestDistance)
		{
			BestDistance = Distance;
			Pickup = *It;
		}
	}
	if (Pickup != nullptr)
	{
		return Pickup->TryCollect(Knight, FGuid::NewGuid());
	}
	for (AKnightRunestone* Candidate : Runestones)
	{
		const double Distance = FVector::DistSquared(Knight->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance <= BestDistance)
		{
			BestDistance = Distance;
			Stone = Candidate;
		}
	}
	if (Stone != nullptr)
	{
		return Stone->TryActivate(Knight, FGuid::NewGuid());
	}
	for (TActorIterator<AKnightItemPickup> It(GetWorld()); It; ++It)
	{
		if (!It->IsClaimed() && FVector::DistSquared(Knight->GetActorLocation(), It->GetActorLocation())
			<= FMath::Square(static_cast<double>(KnightRPG::PickupRange)))
		{
			return Super::TryInteract(Player);
		}
	}
	// Isolated damage fixture at the sanctuary: deliberate hurt command for healer/Runestone testing.
	if (IsInUpgradeSafeZone(Knight))
	{
		return UKnightAbilitySystemComponent::ApplyDamage(Knight, Knight, 150.0f);
	}
	return Super::TryInteract(Player);
}

void AKnightSkillLabGameMode::ResetLabFixtures()
{
	EncounterId = FGuid::NewGuid();
	ResetTrainingDummy();
	for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		if (UKnightSkillRuntimeComponent* Runtime = Entry.Knight->FindComponentByClass<UKnightSkillRuntimeComponent>())
		{
			Runtime->SetEncounterId(EncounterId);
		}
	}
	for (AKnightRunestone* Stone : Runestones)
	{
		Stone->ResetFixture(EncounterId);
	}
	if (FrozenSurface != nullptr)
	{
		FrozenSurface->ResetSurface();
	}
}

FKnightResult AKnightSkillLabGameMode::TryRestartDrill(APlayerController* Player)
{
	const FKnightResult Result = Super::TryRestartDrill(Player);
	if (Result.IsSuccess())
	{
		ResetLabFixtures();
	}
	return Result;
}

FGuid AKnightSkillLabGameMode::GetLabEncounterId() const
{
	return EncounterId;
}

void AKnightSkillLabGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(DummyReadinessTimer);
	if (GetArenaState() != nullptr)
	{
		GetArenaState()->OnSnapshotChanged.Remove(ArenaHandle);
	}
	Super::EndPlay(EndPlayReason);
}
