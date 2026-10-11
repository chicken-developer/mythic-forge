#include "GearLab/KnightGearLabGameMode.h"

#include "Attributes/KnightAttributeSet.h"
#include "Catalog/KnightGearCatalog.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GearLab/KnightGearLabHUD.h"
#include "GearLab/KnightGearLabPlayerController.h"
#include "GearLab/KnightGearLabState.h"
#include "GearLab/KnightGearSnapshotSubsystem.h"
#include "Engine/GameInstance.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Party/KnightPartyState.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "PartyArena/KnightPartyArenaStartMarker.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "Tags/KnightFixtureGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightGearLab, Log, All);

AKnightGearLabGameMode::AKnightGearLabGameMode()
{
	PlayerControllerClass = AKnightGearLabPlayerController::StaticClass();
	HUDClass = AKnightGearLabHUD::StaticClass();
}

bool AKnightGearLabGameMode::IsCatalogLab() const
{
	return false;
}

int32 AKnightGearLabGameMode::GetInitialInventoryCapacity() const
{
	return KnightGear::DemoCapacity;
}

bool AKnightGearLabGameMode::ShouldIssueLegacyEncounterRewards() const
{
	return false;
}

bool AKnightGearLabGameMode::ShouldAdvanceWaveToBoss() const
{
	return false;
}

void AKnightGearLabGameMode::InitGameState()
{
	UKnightGearSnapshotSubsystem* Store = GetGameInstance()->GetSubsystem<UKnightGearSnapshotSubsystem>();
	const bool bReconstructing = Store->HasPendingReconstruction();
	const FKnightGearDemoSnapshot Fixture = bReconstructing ? Store->GetFixtureSnapshot() : FKnightGearDemoSnapshot();
	DemoRunId = bReconstructing ? Fixture.DemoRunId : FGuid::NewGuid();
	CompletedEncounters = Fixture.CompletedEncounters;
	PendingRewardId = Fixture.ClaimableRewardId;
	Super::InitGameState();
	if (GetRPGInitializationResult().IsSuccess())
	{
		GearState = GetWorld()->SpawnActor<AKnightGearLabState>();
		RefreshFixture();
		if (bReconstructing)
		{
			for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
			{
				const FKnightResult RestoredPoise = Entry.Knight->GetCombatComponent()->RestoreSnapshotPoise(
					Store->GetCapturedPoise(Entry.KnightId));
				if (!RestoredPoise.IsSuccess())
				{
					UE_LOG(LogKnightGearLab, Error, TEXT("Fresh gear snapshot Poise restore failed: %s"),
						*RestoredPoise.Context.ToString());
					return;
				}
			}
			Store->ConsumeReconstruction();
			UE_LOG(LogKnightGearLab, Display, TEXT("Gear fixture reconstructed fresh canonical domains: same run%s, receipts, GUIDs and remaining timers."),
				*DemoRunId.ToString());
		}
	}
}

FKnightEntityId AKnightGearLabGameMode::CreateKnightIdentity(const int32 Slot, const FPrimaryAssetId& HeroId) const
{
	const UKnightGearSnapshotSubsystem* Store = GetGameInstance()->GetSubsystem<UKnightGearSnapshotSubsystem>();
	return Store->HasPendingReconstruction() ? Store->FindCanonicalIdentity(HeroId)
		: Super::CreateKnightIdentity(Slot, HeroId);
}

FKnightResult AKnightGearLabGameMode::InitializeRPGDomains(UKnightInventoryComponent* Inventory,
	UKnightRPGComponent* RPG, TConstArrayView<FKnightRPGKnightRegistration> Registrations)
{
	const UKnightGearSnapshotSubsystem* Store = GetGameInstance()->GetSubsystem<UKnightGearSnapshotSubsystem>();
	return Store->HasPendingReconstruction() ? RPG->TryRestoreSnapshot(Inventory, Store->GetElapsedSnapshot(), Registrations)
		: Super::InitializeRPGDomains(Inventory, RPG, Registrations);
}

FKnightResult AKnightGearLabGameMode::ConfigureRPGDomains(UKnightInventoryComponent* Inventory,
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
	for (const FKnightGearSeed& Seed : FKnightGearCatalog::GetSeeds())
	{
		const FString Key = Seed.CatalogId.ToString();
		const bool bStarter = Key == TEXT("W01") || (Key.StartsWith(TEXT("A"))
			&& FCString::Atoi(*Key.Mid(1)) <= 7);
		if (!IsCatalogLab() && !bStarter && Key != TEXT("C01") && Key != TEXT("C02"))
		{
			continue;
		}
		const int32 Instances = Seed.MaximumStack == 1 ? (Seed.Metadata.bUniqueOwned ? 1 : 3) : 1;
		for (int32 Index = 0; Index < Instances; ++Index)
		{
			FKnightInventoryMutation Mutation;
			Mutation.Item.InstanceId = FGuid::NewGuid();
			Mutation.Item.DefinitionId = Seed.DefinitionId;
			Mutation.Item.Count = Seed.MaximumStack == 1 ? 1
				: (IsCatalogLab() ? FMath::Min(Seed.MaximumStack, 50) : 6);
			Request.Mutations.Add(Mutation);
			if (Request.Mutations.Num() == KnightItems::MaximumBatchOperations)
			{
				Result = Inventory->TryApplyTransaction(Request).Result;
				if (!Result.IsSuccess())
				{
					return Result;
				}
				Request.Mutations.Reset();
				Request.RequestId = FGuid::NewGuid();
				Request.ExpectedRevision = Inventory->GetView().Revision;
			}
		}
	}
	if (!Request.Mutations.IsEmpty())
	{
		Result = Inventory->TryApplyTransaction(Request).Result;
	}
	return Result.IsSuccess() ? RPG->ConfigureGearDemo(DemoRunId, IsCatalogLab() ? 1000 : 30,
		IsCatalogLab() ? 8 : 1, true) : Result;
}

FKnightResult AKnightGearLabGameMode::BuildCaller(APlayerController* Player, FKnightRPGCallerContext& OutCaller) const
{
	return Super::BuildCaller(Player, OutCaller);
}

void AKnightGearLabGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (GetRPGInitializationResult().IsSuccess())
	{
		GearArenaHandle = GetArenaState()->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleGearArenaChanged);
		RefreshFixture();
	}
}

void AKnightGearLabGameMode::HandleGearArenaChanged(const FKnightPartyArenaSnapshot& Snapshot)
{
	if (!IsCatalogLab() && Snapshot.Stage == EKnightArenaStage::Victory
		&& ActiveEncounter == CompletedEncounters && !PendingRewardId.IsValid())
	{
		const FName Encounters[]{TEXT("A"), TEXT("B"), TEXT("C")};
		FKnightGearRewardBundle Bundle = FKnightGearCatalog::Reward(Encounters[ActiveEncounter], DemoRunId,
			GetLabEncounterId());
		const FKnightResult Registered = GetRPGState()->GetRPG()->RegisterRewardBundle(Bundle);
		if (Registered.IsSuccess())
		{
			PendingRewardId = Bundle.RewardId;
			UE_LOG(LogKnightGearLab, Display, TEXT("GearLab success encounter%d run%s reward%s claimable"),
				ActiveEncounter, *DemoRunId.ToString(), *PendingRewardId.ToString());
		}
		else
		{
			UE_LOG(LogKnightGearLab, Error, TEXT("Gear reward registration failed: %s"), *Registered.Context.ToString());
		}
	}
	RefreshFixture();
}

void AKnightGearLabGameMode::RefreshFixture()
{
	if (GearState == nullptr)
	{
		return;
	}
	FKnightGearDemoSnapshot State = GearState->GetSnapshot();
	State.DemoRunId = DemoRunId;
	State.bCatalogLab = IsCatalogLab();
	State.CompletedEncounters = CompletedEncounters;
	State.ClaimableRewardId = PendingRewardId;
	State.StageLabel = IsCatalogLab()
		? NSLOCTEXT("KnightGearLab", "CatalogLabel", "DEV Catalog Lab: level8, all41 definitions, isolated inventory")
		: (PendingRewardId.IsValid()
			? NSLOCTEXT("KnightGearLab", "ClaimLabel", "Demo progression: return to sanctuary, I / Inventory / Claim reward")
			: (CompletedEncounters >= 3
				? NSLOCTEXT("KnightGearLab", "CompleteLabel", "Demo progression complete: level8 / Epic gear. R retries without rewards")
				: FText::Format(NSLOCTEXT("KnightGearLab", "StageLabel", "Demo progression: Encounter {0}/3; F at yellow START"),
					FText::AsNumber(CompletedEncounters + 1))));
	GearState->PublishFixture(State);
}

FKnightRPGCommandResult AKnightGearLabGameMode::ExecuteRPGCommand(AKnightRPGPlayerController* Player,
	const FKnightRPGCommand& Command)
{
	if (bFixtureTravelPending)
	{
		FKnightRPGCommandResult Outcome;
		Outcome.Operation = Command.Operation;
		Outcome.RequestId = Command.RequestId;
		Outcome.KnightId = Command.ExpectedKnightId;
		Outcome.InventoryId = Command.InventoryId;
		Outcome.Result = Command.ValidateShape();
		FKnightRPGCallerContext Caller;
		if (Outcome.Result.IsSuccess())
		{
			Outcome.Result = BuildCaller(Player, Caller);
		}
		if (!Outcome.Result.IsSuccess())
		{
			return Outcome;
		}
		const FKnightRPGCommandResult Committed = GetRPGState()->GetRPG()->QueryCommittedCommand(Caller, Command);
		if (Committed.Result.Error != EKnightError::NotFound)
		{
			return Committed;
		}
		Outcome.Result = FKnightResult::Failure(EKnightError::InvalidState, TEXT("GearFixtureTravelPending"),
			NSLOCTEXT("KnightGearLab", "TravelPending", "The confirmed fixture travel is in progress."));
		return Outcome;
	}
	FKnightRPGCommandResult Result = Super::ExecuteRPGCommand(Player, Command);
	if (Result.Result.IsSuccess() && !Result.bWasReplay && Command.Operation == EKnightRPGOperation::ClaimReward
		&& Command.RewardId == PendingRewardId)
	{
		++CompletedEncounters;
		PendingRewardId.Invalidate();
		RefreshFixture();
	}
	return Result;
}

FKnightResult AKnightGearLabGameMode::StartGearEncounter(APlayerController* Player)
{
	FKnightRPGCallerContext Caller;
	FKnightResult Result = BuildCaller(Player, Caller);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (PendingRewardId.IsValid())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("GearRewardPending"),
			NSLOCTEXT("KnightGearLab", "ClaimFirst", "Claim the complete reward bundle at sanctuary before the next encounter."));
	}
	AKnightPartyArenaStartMarker* Marker = nullptr;
	for (TActorIterator<AKnightPartyArenaStartMarker> It(GetWorld()); It; ++It)
	{
		if (Marker != nullptr)
		{
			return FKnightResult::Failure(EKnightError::InvalidData, TEXT("GearStartMarker"), FText::GetEmpty());
		}
		Marker = *It;
	}
	if (Marker == nullptr || FVector::DistSquared(Caller.Pawn->GetActorLocation(), Marker->GetActorLocation())
		> FMath::Square(Marker->GetInteractionRange()))
	{
		return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("GearStart"),
			NSLOCTEXT("KnightGearLab", "StartMarker", "Move to the yellow START marker and press F."));
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(KnightGearStart), false, Caller.Pawn.Get());
	Query.AddIgnoredActor(Marker);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Caller.Pawn->GetActorLocation(),
		Marker->GetActorLocation() + FVector(0.0, 0.0, 75.0), ECC_Visibility, Query))
	{
		return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("GearStartOccluded"),
			NSLOCTEXT("KnightGearLab", "StartVisible", "The START marker must be visible."));
	}
	const EKnightArenaStage Stage = GetArenaState()->GetSnapshot().Stage;
	if (Stage == EKnightArenaStage::Victory)
	{
		Result = Super::TryRestartDrill(Player);
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	ActiveEncounter = FMath::Min(CompletedEncounters, 2);
	Result = StartFixtureEncounter(ActiveEncounter == 2 ? EKnightArenaStage::Boss : EKnightArenaStage::Wave,
		ActiveEncounter == 1);
	if (Result.IsSuccess())
	{
		const float Health = ActiveEncounter == 2 ? BossEnemyHealth
			: ActiveEncounter == 1 ? EliteEnemyHealth : WaveEnemyHealth;
		for (AKnightArenaEnemy* Enemy : GetEncounterEnemies())
		{
			Enemy->GetKnightAbilitySystem()->SetNumericAttributeBase(UKnightAttributeSet::GetMaxHealthAttribute(), Health);
			Enemy->GetKnightAbilitySystem()->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(), Health);
		}
	}
	RefreshFixture();
	return Result;
}

FKnightResult AKnightGearLabGameMode::TryInteract(APlayerController* Player)
{
	if (bFixtureTravelPending)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("GearFixtureTravelPending"), FText::GetEmpty());
	}
	const EKnightArenaStage Stage = GetArenaState()->GetSnapshot().Stage;
	if (Stage == EKnightArenaStage::AwaitingStart || Stage == EKnightArenaStage::Victory)
	{
		for (TActorIterator<AKnightPartyArenaStartMarker> It(GetWorld()); It; ++It)
		{
			if (IsValid(Player) && Player->GetPawn() != nullptr
				&& FVector::DistSquared(Player->GetPawn()->GetActorLocation(), It->GetActorLocation())
				<= FMath::Square(It->GetInteractionRange()))
			{
				return StartGearEncounter(Player);
			}
		}
	}
	return Super::TryInteract(Player);
}

FKnightResult AKnightGearLabGameMode::TryRestartDrill(APlayerController* Player)
{
	if (bFixtureTravelPending)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("GearFixtureTravelPending"), FText::GetEmpty());
	}
	const FKnightResult Result = Super::TryRestartDrill(Player);
	if (Result.IsSuccess())
	{
		ActiveEncounter = INDEX_NONE;
		RefreshFixture();
	}
	return Result;
}

FKnightResult AKnightGearLabGameMode::ExecuteFixtureAction(APlayerController* Player, const FName Action,
	const FGuid& RequestId)
{
	FKnightRPGCallerContext Caller;
	FKnightResult Result = BuildCaller(Player, Caller);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!RequestId.IsValid() || !Caller.bInSafeZone || Caller.bInCombat)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("GearFixtureSanctuary"),
			NSLOCTEXT("KnightGearLab", "FixtureSanctuary", "Fixture actions require a living Knight at the sanctuary, outside combat."));
	}
	if (const FName* Previous = FixtureRequests.Find(RequestId))
	{
		return *Previous == Action ? FixtureResults[RequestId]
			: FKnightResult::Failure(EKnightError::Conflict, TEXT("GearFixtureReplay"), FText::GetEmpty());
	}
	if (bFixtureTravelPending)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("GearFixtureTravelPending"),
			NSLOCTEXT("KnightGearLab", "TravelPending", "The confirmed fixture travel is in progress."));
	}
	if (Action == TEXT("SanctuaryRecovery"))
	{
		Caller.AbilitySystem->SetNumericAttributeBase(UKnightAttributeSet::GetHealthAttribute(),
			Caller.AbilitySystem->GetNumericAttribute(UKnightAttributeSet::GetMaxHealthAttribute()));
		Result = FKnightResult::Success();
	}
	else if (Action == TEXT("SnapshotReconstruct") && Player->IsLocalController())
	{
		FKnightRPGSnapshot Snapshot;
		Result = GetRPGState()->GetRPG()->CaptureSnapshot(Snapshot);
		UKnightGearSnapshotSubsystem* Store = GetGameInstance()->GetSubsystem<UKnightGearSnapshotSubsystem>();
		if (Result.IsSuccess())
		{
			Result = Store->StageReconstruction(Snapshot, GearState->GetSnapshot());
		}
		if (Result.IsSuccess())
		{
			const FString Map = IsCatalogLab() ? TEXT("/Game/Knight/Maps/Playables/L_M05_6_CatalogLab")
				: TEXT("/Game/Knight/Maps/Playables/L_M05_6_GearLab");
			// A fresh ability after capture would otherwise reconstruct an earlier cooldown.
			// Own a loose-tag count until travel; synchronous transaction fences keep their original scope.
			bFixtureTravelPending = true;
			GuardSnapshotTravelAbilities();
			if (!GetWorld()->ServerTravel(Map + (GetNetMode() == NM_ListenServer ? TEXT("?listen") : TEXT(""))))
			{
				bFixtureTravelPending = false;
				Store->ConsumeReconstruction();
				ReleaseSnapshotTravelAbilities();
				Result = FKnightResult::Failure(EKnightError::InvalidState, TEXT("GearSnapshotTravel"), FText::GetEmpty());
			}
		}
	}
	else if ((Action == TEXT("NewDemoRun") || Action == TEXT("CatalogLab") || Action == TEXT("ResetCatalogLab"))
		&& Player->IsLocalController())
	{
		const FString Map = Action == TEXT("NewDemoRun") ? TEXT("/Game/Knight/Maps/Playables/L_M05_6_GearLab")
			: TEXT("/Game/Knight/Maps/Playables/L_M05_6_CatalogLab");
		bFixtureTravelPending = true;
		Result = GetWorld()->ServerTravel(Map + (GetNetMode() == NM_ListenServer ? TEXT("?listen") : TEXT("")))
			? FKnightResult::Success() : FKnightResult::Failure(EKnightError::InvalidState,
				TEXT("GearFixtureTravel"), FText::GetEmpty());
		bFixtureTravelPending = Result.IsSuccess();
	}
	else
	{
		Result = FKnightResult::Failure(EKnightError::NotOwned, TEXT("GearFixtureAction"), FText::GetEmpty());
	}
	FixtureRequests.Add(RequestId, Action);
	FixtureResults.Add(RequestId, Result);
	return Result;
}

AKnightGearLabState* AKnightGearLabGameMode::GetGearLabState() const
{
	return GearState;
}

void AKnightGearLabGameMode::GuardSnapshotTravelAbilities()
{
	for (const FKnightRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		if (AKnightCharacter* Knight = Entry.Knight.Get())
		{
			UKnightAbilitySystemComponent* ASC = Knight->GetKnightAbilitySystem();
			if (IsValid(ASC) && !SnapshotTravelAbilitySystems.Contains(ASC))
			{
				SnapshotTravelAbilitySystems.Add(ASC);
				ASC->AddLooseGameplayTag(KnightGameplayTags::State_Fixture_TravelPending);
			}
		}
	}
}

void AKnightGearLabGameMode::ReleaseSnapshotTravelAbilities()
{
	for (const TWeakObjectPtr<UKnightAbilitySystemComponent>& WeakASC : SnapshotTravelAbilitySystems)
	{
		if (UKnightAbilitySystemComponent* ASC = WeakASC.Get())
		{
			ASC->RemoveLooseGameplayTag(KnightGameplayTags::State_Fixture_TravelPending);
		}
	}
	SnapshotTravelAbilitySystems.Reset();
}

void AKnightGearLabGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	ReleaseSnapshotTravelAbilities();
	if (GetArenaState() != nullptr)
	{
		GetArenaState()->OnSnapshotChanged.Remove(GearArenaHandle);
	}
	Super::EndPlay(Reason);
}
