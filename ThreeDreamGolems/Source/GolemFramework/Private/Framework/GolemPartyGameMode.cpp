#include "Framework/GolemPartyGameMode.h"

#include "Assets/GolemAssetManager.h"
#include "Character/GolemCharacter.h"
#include "Components/GolemLifeComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Definitions/GolemHeroDefinition.h"
#include "Definitions/GolemPartyDefinition.h"
#include "Framework/GolemGameState.h"
#include "Logging/GolemLog.h"
#include "Party/GolemPartyState.h"
#include "Player/GolemPlayerController.h"
#include "Player/GolemPlayerState.h"
#include "Settings/GolemSettings.h"
#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "String/LexFromString.h"

#define LOCTEXT_NAMESPACE "GolemPartyGameMode"

namespace
{
	struct FGolemMovementSnapshot
	{
		FTransform Transform;
		FVector Velocity;
		EMovementMode MovementMode = MOVE_None;
		uint8 CustomMovementMode = 0;

		explicit FGolemMovementSnapshot(const AGolemCharacter* Golem)
			: Transform(Golem->GetActorTransform())
			, Velocity(Golem->GetCharacterMovement()->Velocity)
			, MovementMode(Golem->GetCharacterMovement()->MovementMode)
			, CustomMovementMode(Golem->GetCharacterMovement()->CustomMovementMode)
		{
		}

		void Restore(AGolemCharacter* Golem) const
		{
			Golem->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
			Golem->GetCharacterMovement()->SetMovementMode(MovementMode, CustomMovementMode);
			Golem->GetCharacterMovement()->Velocity = Velocity;
		}
	};

	bool RestoreControl(APlayerController* Player, AGolemCharacter* PreviousGolem,
		AGolemCharacter* TargetGolem, AAIController* Companion)
	{
		Player->UnPossess();
		Companion->UnPossess();
		Player->Possess(PreviousGolem);
		Companion->Possess(TargetGolem);
		return Player->GetPawn() == PreviousGolem && PreviousGolem->GetController() == Player
			&& Companion->GetPawn() == TargetGolem && TargetGolem->GetController() == Companion;
	}
}

AGolemPartyGameMode::AGolemPartyGameMode()
{
	GameStateClass = AGolemGameState::StaticClass();
	PlayerStateClass = AGolemPlayerState::StaticClass();
	PlayerControllerClass = AGolemPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	GolemCharacterClass = AGolemCharacter::StaticClass();
	AIControllerClass = AAIController::StaticClass();
	PartyDefinitionId = FPrimaryAssetId(TEXT("GolemParty"), TEXT("ThreeGolems"));
}

void AGolemPartyGameMode::InitGameState()
{
	Super::InitGameState();
	if (GameSession != nullptr)
	{
		GameSession->MaxPlayers = UGolemSettings::CanonicalPartySize;
	}
	InitializationResult = EnsureCanonicalParty();
	if (!InitializationResult.IsSuccess())
	{
		UE_LOG(LogGolemCore, Error, TEXT("Canonical party initialization failed [%s]: %s"),
			*PartyDefinitionId.ToString(), *InitializationResult.Message.ToString());
	}
}

void AGolemPartyGameMode::PreLogin(const FString& Options, const FString& Address,
	const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}
	if (!InitializationResult.IsSuccess() || PartyState == nullptr)
	{
		ErrorMessage = TEXT("GolemPartyNotReady");
		return;
	}
	if (PartyState->GetHumanCount() >= UGolemSettings::CanonicalPartySize)
	{
		ErrorMessage = TEXT("GolemPartyFull: the campaign supports at most three humans.");
		return;
	}
	const FString SlotOption = UGameplayStatics::ParseOption(Options, TEXT("GolemSlot"));
	if (!SlotOption.IsEmpty())
	{
		int32 RequestedSlot = INDEX_NONE;
		if (!LexTryParseString(RequestedSlot, *SlotOption)
			|| RequestedSlot < 0 || RequestedSlot >= UGolemSettings::CanonicalPartySize)
		{
			ErrorMessage = TEXT("GolemSlotInvalid: choose a slot from 0 through 2.");
			return;
		}
		const FGolemRosterEntry* Entry = PartyState->FindSlot(RequestedSlot);
		if (Entry == nullptr || Entry->OwnerKind == EGolemControllerOwner::Human)
		{
			ErrorMessage = TEXT("GolemSlotOccupied: the requested Golem is controlled by another human.");
		}
	}
}

FString AGolemPartyGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	const FString ParentError = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!ParentError.IsEmpty())
	{
		return ParentError;
	}
	int32 DesiredSlot = INDEX_NONE;
	const FString SlotOption = UGameplayStatics::ParseOption(Options, TEXT("GolemSlot"));
	if (!SlotOption.IsEmpty()
		&& (!LexTryParseString(DesiredSlot, *SlotOption) || DesiredSlot < 0
			|| DesiredSlot >= UGolemSettings::CanonicalPartySize))
	{
		return TEXT("GolemSlotInvalid");
	}
	DesiredLoginSlots.Add(NewPlayerController, DesiredSlot);
	return FString();
}

void AGolemPartyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	RestartPlayer(NewPlayer);
}

void AGolemPartyGameMode::RestartPlayer(AController* NewPlayer)
{
	APlayerController* Player = Cast<APlayerController>(NewPlayer);
	if (Player == nullptr)
	{
		return;
	}
	if (PartyState != nullptr && PartyState->GetControlledSlot(Player) != INDEX_NONE)
	{
		return;
	}
	const int32* DesiredSlot = DesiredLoginSlots.Find(Player);
	const FGolemResult Result = AssignHumanSlot(Player, DesiredSlot != nullptr ? *DesiredSlot : INDEX_NONE);
	if (!Result.IsSuccess())
	{
		RejectHumanAssignment(Player, Result);
		return;
	}
	DesiredLoginSlots.Remove(Player);
}

void AGolemPartyGameMode::Logout(AController* Exiting)
{
	APlayerController* Player = Cast<APlayerController>(Exiting);
	if (Player != nullptr)
	{
		DesiredLoginSlots.Remove(Player);
	}
	const int32 Slot = PartyState != nullptr ? PartyState->GetControlledSlot(Exiting) : INDEX_NONE;
	const FGolemRosterEntry* Entry = PartyState != nullptr ? PartyState->FindSlot(Slot) : nullptr;
	AGolemCharacter* Golem = Entry != nullptr ? Entry->Golem.Get() : nullptr;
	if (Golem != nullptr && !GetWorld()->bIsTearingDown)
	{
		const FGolemMovementSnapshot Snapshot(Golem);
		Exiting->UnPossess();
		AAIController* Companion = SpawnCompanionController(Golem);
		Snapshot.Restore(Golem);
		if (Companion != nullptr && Companion->GetPawn() == Golem)
		{
			const TArray<int32> Slots { Slot };
			const FGolemResult Result = PartyState->RefreshControllerOwnership(Slots);
			if (!Result.IsSuccess())
			{
				UE_LOG(LogGolemCore, Error, TEXT("AI takeover ownership commit failed slot %d: %s"),
					Slot, *Result.Message.ToString());
			}
			UE_LOG(LogGolemCore, Display, TEXT("AI takeover slot %d Golem %s (canonical count %d)"),
				Slot, *Golem->GetGolemEntityId().Value.ToString(), PartyState->GetRoster().Num());
		}
		else
		{
			UE_LOG(LogGolemCore, Error, TEXT("AI takeover failed slot %d Golem %s; actor preserved"),
				Slot, *Golem->GetGolemEntityId().Value.ToString());
		}
	}
	if (AGolemPlayerState* Human = Exiting != nullptr ? Exiting->GetPlayerState<AGolemPlayerState>() : nullptr)
	{
		Human->SetAssignedSlot(INDEX_NONE);
	}
	Super::Logout(Exiting);
}

AGolemPartyState* AGolemPartyGameMode::GetPartyState() const
{
	return PartyState;
}

const FGolemResult& AGolemPartyGameMode::GetInitializationResult() const
{
	return InitializationResult;
}

FVector AGolemPartyGameMode::GetSpawnLocationForSlot(int32 Slot) const
{
	return SpawnOrigin + FVector(0.0, static_cast<double>(SlotSpacing) * (Slot - 1), 0.0);
}

FGolemResult AGolemPartyGameMode::TrySwitchGolem(APlayerController* Player, int32 DesiredSlot)
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("SwitchGolem"),
			LOCTEXT("SwitchAuthority", "Only the server may transfer Golem control."));
	}
	if (!IsValid(Player) || Player->GetWorld() != GetWorld() || PartyState == nullptr
		|| !PartyState->IsCanonicalRosterReady())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("SwitchGolem"),
			LOCTEXT("SwitchNotReady", "The human controller and canonical party must be ready before switching."));
	}
	if (PartyState->GetHumanCount() != 1)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("SwitchGolem"),
			LOCTEXT("SwitchSoloOnly", "Golem switching is available when exactly one human controls the party."));
	}
	const int32 CurrentSlot = PartyState->GetControlledSlot(Player);
	const FGolemRosterEntry* PreviousEntry = PartyState->FindSlot(CurrentSlot);
	if (PreviousEntry == nullptr || PreviousEntry->OwnerKind != EGolemControllerOwner::Human
		|| PreviousEntry->Golem != Player->GetPawn())
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("SwitchGolem"),
			LOCTEXT("SwitchOwnership", "The caller does not own a canonical Golem."));
	}
	if (DesiredSlot == INDEX_NONE)
	{
		DesiredSlot = (CurrentSlot + 1) % UGolemSettings::CanonicalPartySize;
	}
	if (DesiredSlot < 0 || DesiredSlot >= UGolemSettings::CanonicalPartySize)
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("SwitchGolem"),
			LOCTEXT("SwitchSlotRange", "The requested Golem slot does not exist."));
	}
	if (DesiredSlot == CurrentSlot)
	{
		return FGolemResult::Success();
	}
	const FGolemRosterEntry* TargetEntry = PartyState->FindSlot(DesiredSlot);
	AGolemCharacter* PreviousGolem = PreviousEntry->Golem;
	AGolemCharacter* TargetGolem = TargetEntry != nullptr ? TargetEntry->Golem.Get() : nullptr;
	AAIController* Companion = TargetGolem != nullptr ? Cast<AAIController>(TargetGolem->GetController()) : nullptr;
	if (TargetEntry == nullptr || TargetEntry->OwnerKind != EGolemControllerOwner::Companion
		|| !IsValid(Companion) || Companion->GetPawn() != TargetGolem)
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("SwitchGolem"),
			LOCTEXT("SwitchTargetOwned", "The target Golem is not available for human takeover."));
	}
	if (!PreviousGolem->CanTransferControl() || !TargetGolem->CanTransferControl())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("SwitchGolem"),
			LOCTEXT("SwitchStateBlocked", "A Golem's current gameplay state blocks control transfer."));
	}
	AGolemPlayerState* Human = Player->GetPlayerState<AGolemPlayerState>();
	if (Human == nullptr || !Human->GetHumanId().IsValid())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("SwitchGolem"),
			LOCTEXT("SwitchHumanMissing", "The human connection identity is unavailable."));
	}

	const FGolemMovementSnapshot PreviousSnapshot(PreviousGolem);
	const FGolemMovementSnapshot TargetSnapshot(TargetGolem);
	PreviousGolem->ResetControlInput();
	TargetGolem->ResetControlInput();
	Companion->UnPossess();
	Player->UnPossess();
	Player->Possess(TargetGolem);
	Companion->Possess(PreviousGolem);
	const bool bControlTransferred = Player->GetPawn() == TargetGolem && TargetGolem->GetController() == Player
		&& Companion->GetPawn() == PreviousGolem && PreviousGolem->GetController() == Companion;
	PreviousSnapshot.Restore(PreviousGolem);
	TargetSnapshot.Restore(TargetGolem);
	if (!bControlTransferred)
	{
		const bool bRolledBack = RestoreControl(Player, PreviousGolem, TargetGolem, Companion);
		PreviousSnapshot.Restore(PreviousGolem);
		TargetSnapshot.Restore(TargetGolem);
		UE_LOG(LogGolemCore, Error, TEXT("Golem transfer failed; rollback %s"), bRolledBack ? TEXT("succeeded") : TEXT("failed"));
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("SwitchGolem"),
			LOCTEXT("SwitchPossessionFailed", "Controller transfer failed; the previous ownership was restored where possible."));
	}
	const TArray<int32> ChangedSlots { CurrentSlot, DesiredSlot };
	const FGolemResult Commit = PartyState->RefreshControllerOwnership(ChangedSlots);
	if (!Commit.IsSuccess())
	{
		const bool bRolledBack = RestoreControl(Player, PreviousGolem, TargetGolem, Companion);
		PreviousSnapshot.Restore(PreviousGolem);
		TargetSnapshot.Restore(TargetGolem);
		UE_LOG(LogGolemCore, Error, TEXT("Golem ownership commit failed; rollback %s: %s"),
			bRolledBack ? TEXT("succeeded") : TEXT("failed"), *Commit.Message.ToString());
		return Commit;
	}
	Human->SetAssignedSlot(DesiredSlot);
	if (AGolemGameState* State = GetGameState<AGolemGameState>())
	{
		const FGolemResult Progress = State->RecordControlledSlot(Player, DesiredSlot);
		if (!Progress.IsSuccess())
		{
			UE_LOG(LogGolemCore, Error, TEXT("Transferred Golem training record failed: %s"), *Progress.Message.ToString());
		}
	}
	UE_LOG(LogGolemCore, Display, TEXT("Golem transfer %d -> %d; IDs/actors preserved; canonical count %d"),
		CurrentSlot, DesiredSlot, PartyState->GetRoster().Num());
	return FGolemResult::Success();
}

FGolemResult AGolemPartyGameMode::TryRestartDrill(APlayerController* Player)
{
	if (AGolemGameState* State = GetGameState<AGolemGameState>())
	{
		return State->ResetTrainingProgress(Player);
	}
	return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RestartDrill"),
		LOCTEXT("RestartStateMissing", "The training state is unavailable."));
}

FGolemResult AGolemPartyGameMode::TryInteract(APlayerController* Player)
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("Interact"),
			LOCTEXT("InteractAuthority", "Only the server may resolve an interaction."));
	}
	if (!IsValid(Player) || Player->GetWorld() != GetWorld() || PartyState == nullptr
		|| PartyState->GetControlledSlot(Player) == INDEX_NONE)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("Interact"),
			LOCTEXT("InteractOwnership", "Interaction requires an assigned human controlling a canonical Golem."));
	}
	return FGolemResult::Failure(EGolemError::InvalidState, TEXT("Interact"),
		LOCTEXT("InteractUnavailable", "This training mode does not contain interaction stations."));
}

FGolemResult AGolemPartyGameMode::TryRecoverGolemFromFall(AGolemCharacter* Golem)
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("RecoverGolem"),
			LOCTEXT("RecoveryAuthority", "Only the server may recover a fallen canonical Golem."));
	}
	UWorld* World = GetWorld();
	if (World == nullptr || World->bIsTearingDown || World->IsInSeamlessTravel())
	{
		return FGolemResult::Failure(EGolemError::ShuttingDown, TEXT("RecoverGolem"),
			LOCTEXT("RecoveryWorldUnavailable", "Golem recovery is unavailable while the world is changing."));
	}
	if (!IsValid(Golem) || Golem->IsActorBeingDestroyed()
		|| Golem->GetWorld() != World || !Golem->HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("RecoverGolem"),
			LOCTEXT("RecoveryWrongWorld", "Recovery requires a canonical Golem from this authoritative world."));
	}
	const FGolemRosterEntry* Entry = PartyState != nullptr ? PartyState->FindGolem(Golem) : nullptr;
	if (Entry == nullptr || Entry->Slot < 0 || Entry->Slot >= UGolemSettings::CanonicalPartySize
		|| Entry->GolemId != Golem->GetGolemEntityId()
		|| Entry->HeroDefinitionId != Golem->GetHeroDefinitionId()
		|| PartyState->FindSlot(Entry->Slot) != Entry)
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("RecoverGolem"),
			LOCTEXT("RecoveryNotCanonical", "The fallen actor does not match a canonical roster identity and slot."));
	}
	UCharacterMovementComponent* Movement = Golem->GetCharacterMovement();
	if (!PartyState->IsCanonicalRosterReady() || Movement == nullptr)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RecoverGolem"),
			LOCTEXT("RecoveryNotReady", "The existing canonical party and movement component must be ready."));
	}
	const FVector RecoveryLocation = GetSpawnLocationForSlot(Entry->Slot);
	if (RecoveryLocation.ContainsNaN())
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("RecoverGolem"),
			LOCTEXT("RecoveryInvalidLocation", "The configured recovery location is invalid."));
	}
	if (AAIController* Companion = Cast<AAIController>(Golem->GetController()))
	{
		Companion->StopMovement();
	}
	APlayerController* Player = Cast<APlayerController>(Golem->GetController());
	if (AGolemPlayerController* Human = Cast<AGolemPlayerController>(Player))
	{
		Human->ResetControlInput();
	}
	else if (Player != nullptr)
	{
		Player->FlushPressedKeys();
	}
	Golem->GetCombatComponent()->ClearTransientActions();
	Golem->ResetControlInput();
	Movement->StopMovementImmediately();
	if (!Golem->TeleportTo(RecoveryLocation, FRotator::ZeroRotator, false, false))
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("RecoverGolem"),
			LOCTEXT("RecoveryTeleportBlocked", "Collision prevented recovery; the canonical actor was preserved."));
	}
	if (Player != nullptr)
	{
		Player->SetControlRotation(FRotator::ZeroRotator);
		Player->ClientSetRotation(FRotator::ZeroRotator);
	}
	Movement->SetMovementMode(Golem->GetLifeComponent()->IsAlive() ? MOVE_Walking : MOVE_None);
	Golem->ForceNetUpdate();
	UE_LOG(LogGolemCore, Display, TEXT("Recovered fallen Golem %s in slot %d; actor/identity/controller preserved"),
		*Entry->GolemId.Value.ToString(), Entry->Slot);
	return FGolemResult::Success();
}

void AGolemPartyGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DesiredLoginSlots.Reset();
	Super::EndPlay(EndPlayReason);
}

FGolemResult AGolemPartyGameMode::EnsureCanonicalParty()
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("SpawnParty"),
			LOCTEXT("SpawnAuthority", "Clients cannot spawn the canonical party."));
	}
	if (PartyState != nullptr)
	{
		return PartyState->IsCanonicalRosterReady() ? FGolemResult::Success()
			: FGolemResult::Failure(EGolemError::InvalidState, TEXT("SpawnParty"),
				LOCTEXT("ExistingPartyInvalid", "The existing canonical party is invalid and cannot be silently rebuilt."));
	}
	AGolemGameState* State = GetGameState<AGolemGameState>();
	UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
	if (State == nullptr || Manager == nullptr || GolemCharacterClass == nullptr || AIControllerClass == nullptr)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("SpawnParty"),
			LOCTEXT("CompositionMissing", "Party spawning requires configured framework, character, AI and asset manager classes."));
	}
	UGolemDefinition* Definition = nullptr;
	const FGolemResult PartyLoad = Manager->ResolveDefinition(PartyDefinitionId, UGolemPartyDefinition::StaticClass(), Definition);
	if (!PartyLoad.IsSuccess())
	{
		return PartyLoad;
	}
	UGolemPartyDefinition* PartyDefinition = CastChecked<UGolemPartyDefinition>(Definition);
	const FGolemResult PartyValidation = PartyDefinition->ValidateParty();
	if (!PartyValidation.IsSuccess())
	{
		return PartyValidation;
	}
	TArray<UGolemHeroDefinition*> Heroes;
	for (const FPrimaryAssetId& HeroId : PartyDefinition->GetHeroDefinitions())
	{
		const FGolemResult HeroLoad = Manager->ResolveDefinition(HeroId, UGolemHeroDefinition::StaticClass(), Definition);
		if (!HeroLoad.IsSuccess())
		{
			return HeroLoad;
		}
		UGolemHeroDefinition* Hero = CastChecked<UGolemHeroDefinition>(Definition);
		const FGolemResult HeroValidation = Hero->ValidateHero();
		if (!HeroValidation.IsSuccess())
		{
			return HeroValidation;
		}
		Heroes.Add(Hero);
	}

	FActorSpawnParameters PartyParameters;
	PartyParameters.Owner = this;
	AGolemPartyState* NewParty = GetWorld()->SpawnActor<AGolemPartyState>(PartyParameters);
	if (NewParty == nullptr)
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("SpawnParty"),
			LOCTEXT("PartyActorSpawnFailed", "The canonical PartyState actor could not be spawned."));
	}
	TArray<AGolemCharacter*> SpawnedGolems;
	TArray<AAIController*> SpawnedCompanions;
	TArray<FGolemRosterEntry> Entries;
	FGolemResult SpawnResult = FGolemResult::Success();
	for (int32 Slot = 0; Slot < UGolemSettings::CanonicalPartySize; ++Slot)
	{
		const FTransform Transform(FRotator::ZeroRotator, GetSpawnLocationForSlot(Slot));
		AGolemCharacter* Golem = GetWorld()->SpawnActorDeferred<AGolemCharacter>(GolemCharacterClass,
			Transform, NewParty, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (Golem == nullptr)
		{
			SpawnResult = FGolemResult::Failure(EGolemError::InvalidState, TEXT("SpawnParty"),
				LOCTEXT("GolemSpawnFailed", "A canonical Golem could not be spawned."));
			break;
		}
		SpawnedGolems.Add(Golem);
		const FGolemEntityId GolemId = CreateGolemIdentity(Slot, Heroes[Slot]->GetPrimaryAssetId());
		SpawnResult = Golem->InitializeGolem(GolemId, Heroes[Slot]);
		if (!SpawnResult.IsSuccess())
		{
			break;
		}
		Golem->FinishSpawning(Transform);
		AAIController* Companion = SpawnCompanionController(Golem);
		if (Companion == nullptr || Companion->GetPawn() != Golem)
		{
			SpawnResult = FGolemResult::Failure(EGolemError::InvalidState, TEXT("SpawnParty"),
				LOCTEXT("CompanionSpawnFailed", "A canonical Golem could not receive its companion controller."));
			break;
		}
		SpawnedCompanions.Add(Companion);
		FGolemRosterEntry Entry;
		Entry.Slot = Slot;
		Entry.GolemId = GolemId;
		Entry.HeroDefinitionId = Heroes[Slot]->GetPrimaryAssetId();
		Entry.Golem = Golem;
		Entries.Add(Entry);
	}
	if (SpawnResult.IsSuccess())
	{
		SpawnResult = NewParty->InitializeRoster(Entries);
	}
	if (SpawnResult.IsSuccess())
	{
		SpawnResult = State->SetPartyState(NewParty);
	}
	if (!SpawnResult.IsSuccess())
	{
		for (AAIController* Companion : SpawnedCompanions)
		{
			Companion->UnPossess();
			Companion->Destroy();
		}
		for (AGolemCharacter* Golem : SpawnedGolems)
		{
			Golem->Destroy();
		}
		NewParty->Destroy();
		return SpawnResult;
	}
	PartyState = NewParty;
	UE_LOG(LogGolemCore, Display, TEXT("Canonical party created: %s, three Golem actors, three companion controllers"),
		*PartyDefinitionId.ToString());
	return FGolemResult::Success();
}

FGolemEntityId AGolemPartyGameMode::CreateGolemIdentity(const int32 Slot, const FPrimaryAssetId& HeroId) const
{
	return FGolemEntityId::Create();
}

FGolemResult AGolemPartyGameMode::AssignHumanSlot(APlayerController* Player, int32 DesiredSlot)
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("AssignHuman"),
			LOCTEXT("AssignAuthority", "Only the server may assign human Golem ownership."));
	}
	if (!IsValid(Player) || Player->GetWorld() != GetWorld() || PartyState == nullptr
		|| !PartyState->IsCanonicalRosterReady())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("AssignHuman"),
			LOCTEXT("AssignNotReady", "The human and canonical party must be ready before assignment."));
	}
	if (PartyState->GetControlledSlot(Player) != INDEX_NONE)
	{
		return FGolemResult::Success();
	}
	if (PartyState->GetHumanCount() >= UGolemSettings::CanonicalPartySize)
	{
		return FGolemResult::Failure(EGolemError::CapacityExceeded, TEXT("AssignHuman"),
			LOCTEXT("AssignPartyFull", "The campaign already has three human players."));
	}
	if (DesiredSlot == INDEX_NONE)
	{
		for (const FGolemRosterEntry& Candidate : PartyState->GetRoster())
		{
			if (Candidate.OwnerKind == EGolemControllerOwner::Companion)
			{
				DesiredSlot = Candidate.Slot;
				break;
			}
		}
	}
	const FGolemRosterEntry* Entry = PartyState->FindSlot(DesiredSlot);
	if (Entry == nullptr)
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("AssignHuman"),
			LOCTEXT("AssignSlotInvalid", "The requested party slot does not exist."));
	}
	if (Entry->OwnerKind != EGolemControllerOwner::Companion)
	{
		return FGolemResult::Failure(EGolemError::Conflict, TEXT("AssignHuman"),
			LOCTEXT("AssignSlotOccupied", "The requested Golem is already controlled by another human."));
	}
	AGolemPlayerState* Human = Player->GetPlayerState<AGolemPlayerState>();
	AGolemCharacter* Golem = Entry->Golem;
	AAIController* PreviousCompanion = IsValid(Golem) ? Cast<AAIController>(Golem->GetController()) : nullptr;
	if (Human == nullptr || PreviousCompanion == nullptr || !IsValid(Golem)
		|| Golem->GetWorld() != GetWorld() || Golem->IsActorBeingDestroyed()
		|| !Golem->GetGolemEntityId().IsValid() || Golem->GetCharacterMovement() == nullptr
		|| GetWorld()->bIsTearingDown || GetWorld()->IsInSeamlessTravel())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("AssignHuman"),
			LOCTEXT("AssignControllerMissing", "The human identity, existing Golem or companion controller is unavailable."));
	}
	const FGolemResult Identity = Human->InitializeHumanIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	const FGolemMovementSnapshot Snapshot(Golem);
	PreviousCompanion->UnPossess();
	Player->Possess(Golem);
	Snapshot.Restore(Golem);
	if (Player->GetPawn() != Golem || Golem->GetController() != Player)
	{
		Player->UnPossess();
		PreviousCompanion->Possess(Golem);
		Snapshot.Restore(Golem);
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("AssignHuman"),
			LOCTEXT("AssignPossessionFailed", "Human possession failed; companion ownership was restored."));
	}
	const TArray<int32> Slots { DesiredSlot };
	const FGolemResult Commit = PartyState->RefreshControllerOwnership(Slots);
	if (!Commit.IsSuccess())
	{
		Player->UnPossess();
		PreviousCompanion->Possess(Golem);
		Snapshot.Restore(Golem);
		return Commit;
	}
	Human->SetAssignedSlot(DesiredSlot);
	PreviousCompanion->Destroy();
	if (AGolemGameState* State = GetGameState<AGolemGameState>())
	{
		const FGolemResult Progress = State->RecordControlledSlot(Player, DesiredSlot);
		if (!Progress.IsSuccess())
		{
			UE_LOG(LogGolemCore, Error, TEXT("Assigned Golem training record failed: %s"), *Progress.Message.ToString());
		}
	}
	UE_LOG(LogGolemCore, Display, TEXT("Human %d assigned Golem slot %d; %d humans, canonical count %d"),
		Human->GetPlayerId(), DesiredSlot, PartyState->GetHumanCount(), PartyState->GetRoster().Num());
	return FGolemResult::Success();
}

AAIController* AGolemPartyGameMode::SpawnCompanionController(AGolemCharacter* Golem)
{
	if (!HasAuthority() || !IsValid(Golem) || AIControllerClass == nullptr)
	{
		return nullptr;
	}
	if (AAIController* Existing = Cast<AAIController>(Golem->GetController()))
	{
		return Existing;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	AAIController* Companion = GetWorld()->SpawnActor<AAIController>(AIControllerClass,
		FVector::ZeroVector, FRotator::ZeroRotator, Parameters);
	if (Companion != nullptr)
	{
		Companion->Possess(Golem);
		if (Companion->GetPawn() != Golem)
		{
			Companion->Destroy();
			return nullptr;
		}
	}
	return Companion;
}

void AGolemPartyGameMode::RejectHumanAssignment(APlayerController* Player, const FGolemResult& Result)
{
	UE_LOG(LogGolemCore, Error, TEXT("Human assignment rejected [%s]: %s"),
		*Result.Context.ToString(), *Result.Message.ToString());
	if (GameSession != nullptr && Player != nullptr)
	{
		if (!GameSession->KickPlayer(Player, Result.Message))
		{
			Player->ClientReturnToMainMenuWithTextReason(Result.Message);
			Player->Destroy();
		}
	}
}

#undef LOCTEXT_NAMESPACE
