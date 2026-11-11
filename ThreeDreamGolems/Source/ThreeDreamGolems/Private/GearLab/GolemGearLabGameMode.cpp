#include "GearLab/GolemGearLabGameMode.h"

#include "Attributes/GolemAttributeSet.h"
#include "Catalog/GolemGearCatalog.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GearLab/GolemGearLabHUD.h"
#include "GearLab/GolemGearLabPlayerController.h"
#include "GearLab/GolemGearLabState.h"
#include "GearLab/GolemGearSnapshotSubsystem.h"
#include "Engine/GameInstance.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Party/GolemPartyState.h"
#include "PartyArena/GolemArenaEnemy.h"
#include "PartyArena/GolemPartyArenaStartMarker.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "Tags/GolemFixtureGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemGearLab, Log, All);

AGolemGearLabGameMode::AGolemGearLabGameMode()
{
	PlayerControllerClass = AGolemGearLabPlayerController::StaticClass();
	HUDClass = AGolemGearLabHUD::StaticClass();
}

bool AGolemGearLabGameMode::IsCatalogLab() const
{
	return false;
}

int32 AGolemGearLabGameMode::GetInitialInventoryCapacity() const
{
	return GolemGear::DemoCapacity;
}

bool AGolemGearLabGameMode::ShouldIssueLegacyEncounterRewards() const
{
	return false;
}

bool AGolemGearLabGameMode::ShouldAdvanceWaveToBoss() const
{
	return false;
}

void AGolemGearLabGameMode::InitGameState()
{
	UGolemGearSnapshotSubsystem* Store = GetGameInstance()->GetSubsystem<UGolemGearSnapshotSubsystem>();
	const bool bReconstructing = Store->HasPendingReconstruction();
	const FGolemGearDemoSnapshot Fixture = bReconstructing ? Store->GetFixtureSnapshot() : FGolemGearDemoSnapshot();
	DemoRunId = bReconstructing ? Fixture.DemoRunId : FGuid::NewGuid();
	CompletedEncounters = Fixture.CompletedEncounters;
	PendingRewardId = Fixture.ClaimableRewardId;
	Super::InitGameState();
	if (GetRPGInitializationResult().IsSuccess())
	{
		GearState = GetWorld()->SpawnActor<AGolemGearLabState>();
		RefreshFixture();
		if (bReconstructing)
		{
			for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
			{
				const FGolemResult RestoredPoise = Entry.Golem->GetCombatComponent()->RestoreSnapshotPoise(
					Store->GetCapturedPoise(Entry.GolemId));
				if (!RestoredPoise.IsSuccess())
				{
					UE_LOG(LogGolemGearLab, Error, TEXT("Fresh gear snapshot Poise restore failed: %s"),
						*RestoredPoise.Context.ToString());
					return;
				}
			}
			Store->ConsumeReconstruction();
			UE_LOG(LogGolemGearLab, Display, TEXT("Gear fixture reconstructed fresh canonical domains: same run%s, receipts, GUIDs and remaining timers."),
				*DemoRunId.ToString());
		}
	}
}

FGolemEntityId AGolemGearLabGameMode::CreateGolemIdentity(const int32 Slot, const FPrimaryAssetId& HeroId) const
{
	const UGolemGearSnapshotSubsystem* Store = GetGameInstance()->GetSubsystem<UGolemGearSnapshotSubsystem>();
	return Store->HasPendingReconstruction() ? Store->FindCanonicalIdentity(HeroId)
		: Super::CreateGolemIdentity(Slot, HeroId);
}

FGolemResult AGolemGearLabGameMode::InitializeRPGDomains(UGolemInventoryComponent* Inventory,
	UGolemRPGComponent* RPG, TConstArrayView<FGolemRPGGolemRegistration> Registrations)
{
	const UGolemGearSnapshotSubsystem* Store = GetGameInstance()->GetSubsystem<UGolemGearSnapshotSubsystem>();
	return Store->HasPendingReconstruction() ? RPG->TryRestoreSnapshot(Inventory, Store->GetElapsedSnapshot(), Registrations)
		: Super::InitializeRPGDomains(Inventory, RPG, Registrations);
}

FGolemResult AGolemGearLabGameMode::ConfigureRPGDomains(UGolemInventoryComponent* Inventory,
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
	for (const FGolemGearSeed& Seed : FGolemGearCatalog::GetSeeds())
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
			FGolemInventoryMutation Mutation;
			Mutation.Item.InstanceId = FGuid::NewGuid();
			Mutation.Item.DefinitionId = Seed.DefinitionId;
			Mutation.Item.Count = Seed.MaximumStack == 1 ? 1
				: (IsCatalogLab() ? FMath::Min(Seed.MaximumStack, 50) : 6);
			Request.Mutations.Add(Mutation);
			if (Request.Mutations.Num() == GolemItems::MaximumBatchOperations)
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

FGolemResult AGolemGearLabGameMode::BuildCaller(APlayerController* Player, FGolemRPGCallerContext& OutCaller) const
{
	return Super::BuildCaller(Player, OutCaller);
}

void AGolemGearLabGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (GetRPGInitializationResult().IsSuccess())
	{
		GearArenaHandle = GetArenaState()->OnSnapshotChanged.AddUObject(this, &ThisClass::HandleGearArenaChanged);
		RefreshFixture();
	}
}

void AGolemGearLabGameMode::HandleGearArenaChanged(const FGolemPartyArenaSnapshot& Snapshot)
{
	if (!IsCatalogLab() && Snapshot.Stage == EGolemArenaStage::Victory
		&& ActiveEncounter == CompletedEncounters && !PendingRewardId.IsValid())
	{
		const FName Encounters[]{TEXT("A"), TEXT("B"), TEXT("C")};
		FGolemGearRewardBundle Bundle = FGolemGearCatalog::Reward(Encounters[ActiveEncounter], DemoRunId,
			GetLabEncounterId());
		const FGolemResult Registered = GetRPGState()->GetRPG()->RegisterRewardBundle(Bundle);
		if (Registered.IsSuccess())
		{
			PendingRewardId = Bundle.RewardId;
			UE_LOG(LogGolemGearLab, Display, TEXT("GearLab success encounter%d run%s reward%s claimable"),
				ActiveEncounter, *DemoRunId.ToString(), *PendingRewardId.ToString());
		}
		else
		{
			UE_LOG(LogGolemGearLab, Error, TEXT("Gear reward registration failed: %s"), *Registered.Context.ToString());
		}
	}
	RefreshFixture();
}

void AGolemGearLabGameMode::RefreshFixture()
{
	if (GearState == nullptr)
	{
		return;
	}
	FGolemGearDemoSnapshot State = GearState->GetSnapshot();
	State.DemoRunId = DemoRunId;
	State.bCatalogLab = IsCatalogLab();
	State.CompletedEncounters = CompletedEncounters;
	State.ClaimableRewardId = PendingRewardId;
	State.StageLabel = IsCatalogLab()
		? NSLOCTEXT("GolemGearLab", "CatalogLabel", "DEV Catalog Lab: level8, all41 definitions, isolated inventory")
		: (PendingRewardId.IsValid()
			? NSLOCTEXT("GolemGearLab", "ClaimLabel", "Demo progression: return to sanctuary, I / Inventory / Claim reward")
			: (CompletedEncounters >= 3
				? NSLOCTEXT("GolemGearLab", "CompleteLabel", "Demo progression complete: level8 / Epic gear. R retries without rewards")
				: FText::Format(NSLOCTEXT("GolemGearLab", "StageLabel", "Demo progression: Encounter {0}/3; F at yellow START"),
					FText::AsNumber(CompletedEncounters + 1))));
	GearState->PublishFixture(State);
}

FGolemRPGCommandResult AGolemGearLabGameMode::ExecuteRPGCommand(AGolemRPGPlayerController* Player,
	const FGolemRPGCommand& Command)
{
	if (bFixtureTravelPending)
	{
		FGolemRPGCommandResult Outcome;
		Outcome.Operation = Command.Operation;
		Outcome.RequestId = Command.RequestId;
		Outcome.GolemId = Command.ExpectedGolemId;
		Outcome.InventoryId = Command.InventoryId;
		Outcome.Result = Command.ValidateShape();
		FGolemRPGCallerContext Caller;
		if (Outcome.Result.IsSuccess())
		{
			Outcome.Result = BuildCaller(Player, Caller);
		}
		if (!Outcome.Result.IsSuccess())
		{
			return Outcome;
		}
		const FGolemRPGCommandResult Committed = GetRPGState()->GetRPG()->QueryCommittedCommand(Caller, Command);
		if (Committed.Result.Error != EGolemError::NotFound)
		{
			return Committed;
		}
		Outcome.Result = FGolemResult::Failure(EGolemError::InvalidState, TEXT("GearFixtureTravelPending"),
			NSLOCTEXT("GolemGearLab", "TravelPending", "The confirmed fixture travel is in progress."));
		return Outcome;
	}
	FGolemRPGCommandResult Result = Super::ExecuteRPGCommand(Player, Command);
	if (Result.Result.IsSuccess() && !Result.bWasReplay && Command.Operation == EGolemRPGOperation::ClaimReward
		&& Command.RewardId == PendingRewardId)
	{
		++CompletedEncounters;
		PendingRewardId.Invalidate();
		RefreshFixture();
	}
	return Result;
}

FGolemResult AGolemGearLabGameMode::StartGearEncounter(APlayerController* Player)
{
	FGolemRPGCallerContext Caller;
	FGolemResult Result = BuildCaller(Player, Caller);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (PendingRewardId.IsValid())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("GearRewardPending"),
			NSLOCTEXT("GolemGearLab", "ClaimFirst", "Claim the complete reward bundle at sanctuary before the next encounter."));
	}
	AGolemPartyArenaStartMarker* Marker = nullptr;
	for (TActorIterator<AGolemPartyArenaStartMarker> It(GetWorld()); It; ++It)
	{
		if (Marker != nullptr)
		{
			return FGolemResult::Failure(EGolemError::InvalidData, TEXT("GearStartMarker"), FText::GetEmpty());
		}
		Marker = *It;
	}
	if (Marker == nullptr || FVector::DistSquared(Caller.Pawn->GetActorLocation(), Marker->GetActorLocation())
		> FMath::Square(Marker->GetInteractionRange()))
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("GearStart"),
			NSLOCTEXT("GolemGearLab", "StartMarker", "Move to the yellow START marker and press F."));
	}
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GolemGearStart), false, Caller.Pawn.Get());
	Query.AddIgnoredActor(Marker);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Caller.Pawn->GetActorLocation(),
		Marker->GetActorLocation() + FVector(0.0, 0.0, 75.0), ECC_Visibility, Query))
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("GearStartOccluded"),
			NSLOCTEXT("GolemGearLab", "StartVisible", "The START marker must be visible."));
	}
	const EGolemArenaStage Stage = GetArenaState()->GetSnapshot().Stage;
	if (Stage == EGolemArenaStage::Victory)
	{
		Result = Super::TryRestartDrill(Player);
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	ActiveEncounter = FMath::Min(CompletedEncounters, 2);
	Result = StartFixtureEncounter(ActiveEncounter == 2 ? EGolemArenaStage::Boss : EGolemArenaStage::Wave,
		ActiveEncounter == 1);
	if (Result.IsSuccess())
	{
		const float Health = ActiveEncounter == 2 ? BossEnemyHealth
			: ActiveEncounter == 1 ? EliteEnemyHealth : WaveEnemyHealth;
		for (AGolemArenaEnemy* Enemy : GetEncounterEnemies())
		{
			Enemy->GetGolemAbilitySystem()->SetNumericAttributeBase(UGolemAttributeSet::GetMaxHealthAttribute(), Health);
			Enemy->GetGolemAbilitySystem()->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(), Health);
		}
	}
	RefreshFixture();
	return Result;
}

FGolemResult AGolemGearLabGameMode::TryInteract(APlayerController* Player)
{
	if (bFixtureTravelPending)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("GearFixtureTravelPending"), FText::GetEmpty());
	}
	const EGolemArenaStage Stage = GetArenaState()->GetSnapshot().Stage;
	if (Stage == EGolemArenaStage::AwaitingStart || Stage == EGolemArenaStage::Victory)
	{
		for (TActorIterator<AGolemPartyArenaStartMarker> It(GetWorld()); It; ++It)
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

FGolemResult AGolemGearLabGameMode::TryRestartDrill(APlayerController* Player)
{
	if (bFixtureTravelPending)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("GearFixtureTravelPending"), FText::GetEmpty());
	}
	const FGolemResult Result = Super::TryRestartDrill(Player);
	if (Result.IsSuccess())
	{
		ActiveEncounter = INDEX_NONE;
		RefreshFixture();
	}
	return Result;
}

FGolemResult AGolemGearLabGameMode::ExecuteFixtureAction(APlayerController* Player, const FName Action,
	const FGuid& RequestId)
{
	FGolemRPGCallerContext Caller;
	FGolemResult Result = BuildCaller(Player, Caller);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!RequestId.IsValid() || !Caller.bInSafeZone || Caller.bInCombat)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("GearFixtureSanctuary"),
			NSLOCTEXT("GolemGearLab", "FixtureSanctuary", "Fixture actions require a living Golem at the sanctuary, outside combat."));
	}
	if (const FName* Previous = FixtureRequests.Find(RequestId))
	{
		return *Previous == Action ? FixtureResults[RequestId]
			: FGolemResult::Failure(EGolemError::Conflict, TEXT("GearFixtureReplay"), FText::GetEmpty());
	}
	if (bFixtureTravelPending)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("GearFixtureTravelPending"),
			NSLOCTEXT("GolemGearLab", "TravelPending", "The confirmed fixture travel is in progress."));
	}
	if (Action == TEXT("SanctuaryRecovery"))
	{
		Caller.AbilitySystem->SetNumericAttributeBase(UGolemAttributeSet::GetHealthAttribute(),
			Caller.AbilitySystem->GetNumericAttribute(UGolemAttributeSet::GetMaxHealthAttribute()));
		Result = FGolemResult::Success();
	}
	else if (Action == TEXT("SnapshotReconstruct") && Player->IsLocalController())
	{
		FGolemRPGSnapshot Snapshot;
		Result = GetRPGState()->GetRPG()->CaptureSnapshot(Snapshot);
		UGolemGearSnapshotSubsystem* Store = GetGameInstance()->GetSubsystem<UGolemGearSnapshotSubsystem>();
		if (Result.IsSuccess())
		{
			Result = Store->StageReconstruction(Snapshot, GearState->GetSnapshot());
		}
		if (Result.IsSuccess())
		{
			const FString Map = IsCatalogLab() ? TEXT("/Game/Golem/Maps/Playables/L_M05_6_CatalogLab")
				: TEXT("/Game/Golem/Maps/Playables/L_M05_6_GearLab");
			// A fresh ability after capture would otherwise reconstruct an earlier cooldown.
			// Own a loose-tag count until travel; synchronous transaction fences keep their original scope.
			bFixtureTravelPending = true;
			GuardSnapshotTravelAbilities();
			if (!GetWorld()->ServerTravel(Map + (GetNetMode() == NM_ListenServer ? TEXT("?listen") : TEXT(""))))
			{
				bFixtureTravelPending = false;
				Store->ConsumeReconstruction();
				ReleaseSnapshotTravelAbilities();
				Result = FGolemResult::Failure(EGolemError::InvalidState, TEXT("GearSnapshotTravel"), FText::GetEmpty());
			}
		}
	}
	else if ((Action == TEXT("NewDemoRun") || Action == TEXT("CatalogLab") || Action == TEXT("ResetCatalogLab"))
		&& Player->IsLocalController())
	{
		const FString Map = Action == TEXT("NewDemoRun") ? TEXT("/Game/Golem/Maps/Playables/L_M05_6_GearLab")
			: TEXT("/Game/Golem/Maps/Playables/L_M05_6_CatalogLab");
		bFixtureTravelPending = true;
		Result = GetWorld()->ServerTravel(Map + (GetNetMode() == NM_ListenServer ? TEXT("?listen") : TEXT("")))
			? FGolemResult::Success() : FGolemResult::Failure(EGolemError::InvalidState,
				TEXT("GearFixtureTravel"), FText::GetEmpty());
		bFixtureTravelPending = Result.IsSuccess();
	}
	else
	{
		Result = FGolemResult::Failure(EGolemError::NotOwned, TEXT("GearFixtureAction"), FText::GetEmpty());
	}
	FixtureRequests.Add(RequestId, Action);
	FixtureResults.Add(RequestId, Result);
	return Result;
}

AGolemGearLabState* AGolemGearLabGameMode::GetGearLabState() const
{
	return GearState;
}

void AGolemGearLabGameMode::GuardSnapshotTravelAbilities()
{
	for (const FGolemRosterEntry& Entry : GetPartyState()->GetRoster())
	{
		if (AGolemCharacter* Golem = Entry.Golem.Get())
		{
			UGolemAbilitySystemComponent* ASC = Golem->GetGolemAbilitySystem();
			if (IsValid(ASC) && !SnapshotTravelAbilitySystems.Contains(ASC))
			{
				SnapshotTravelAbilitySystems.Add(ASC);
				ASC->AddLooseGameplayTag(GolemGameplayTags::State_Fixture_TravelPending);
			}
		}
	}
}

void AGolemGearLabGameMode::ReleaseSnapshotTravelAbilities()
{
	for (const TWeakObjectPtr<UGolemAbilitySystemComponent>& WeakASC : SnapshotTravelAbilitySystems)
	{
		if (UGolemAbilitySystemComponent* ASC = WeakASC.Get())
		{
			ASC->RemoveLooseGameplayTag(GolemGameplayTags::State_Fixture_TravelPending);
		}
	}
	SnapshotTravelAbilitySystems.Reset();
}

void AGolemGearLabGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	ReleaseSnapshotTravelAbilities();
	if (GetArenaState() != nullptr)
	{
		GetArenaState()->OnSnapshotChanged.Remove(GearArenaHandle);
	}
	Super::EndPlay(Reason);
}
