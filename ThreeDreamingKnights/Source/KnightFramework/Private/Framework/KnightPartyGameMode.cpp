#include "Framework/KnightPartyGameMode.h"

#include "Assets/KnightAssetManager.h"
#include "Character/KnightCharacter.h"
#include "Components/KnightLifeComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Definitions/KnightHeroDefinition.h"
#include "Definitions/KnightPartyDefinition.h"
#include "Framework/KnightGameState.h"
#include "Logging/KnightLog.h"
#include "Party/KnightPartyState.h"
#include "Player/KnightPlayerController.h"
#include "Player/KnightPlayerState.h"
#include "Settings/KnightSettings.h"
#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "String/LexFromString.h"

#define LOCTEXT_NAMESPACE "KnightPartyGameMode"

namespace
{
	struct FKnightMovementSnapshot
	{
		FTransform Transform;
		FVector Velocity;
		EMovementMode MovementMode = MOVE_None;
		uint8 CustomMovementMode = 0;

		explicit FKnightMovementSnapshot(const AKnightCharacter* Knight)
			: Transform(Knight->GetActorTransform())
			, Velocity(Knight->GetCharacterMovement()->Velocity)
			, MovementMode(Knight->GetCharacterMovement()->MovementMode)
			, CustomMovementMode(Knight->GetCharacterMovement()->CustomMovementMode)
		{
		}

		void Restore(AKnightCharacter* Knight) const
		{
			Knight->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
			Knight->GetCharacterMovement()->SetMovementMode(MovementMode, CustomMovementMode);
			Knight->GetCharacterMovement()->Velocity = Velocity;
		}
	};

	bool RestoreControl(APlayerController* Player, AKnightCharacter* PreviousKnight,
		AKnightCharacter* TargetKnight, AAIController* Companion)
	{
		Player->UnPossess();
		Companion->UnPossess();
		Player->Possess(PreviousKnight);
		Companion->Possess(TargetKnight);
		return Player->GetPawn() == PreviousKnight && PreviousKnight->GetController() == Player
			&& Companion->GetPawn() == TargetKnight && TargetKnight->GetController() == Companion;
	}
}

AKnightPartyGameMode::AKnightPartyGameMode()
{
	GameStateClass = AKnightGameState::StaticClass();
	PlayerStateClass = AKnightPlayerState::StaticClass();
	PlayerControllerClass = AKnightPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	KnightCharacterClass = AKnightCharacter::StaticClass();
	AIControllerClass = AAIController::StaticClass();
	PartyDefinitionId = FPrimaryAssetId(TEXT("KnightParty"), TEXT("TrainingParty"));
}

void AKnightPartyGameMode::InitGameState()
{
	Super::InitGameState();
	if (GameSession != nullptr)
	{
		GameSession->MaxPlayers = UKnightSettings::CanonicalPartySize;
	}
	InitializationResult = EnsureCanonicalParty();
	if (!InitializationResult.IsSuccess())
	{
		UE_LOG(LogKnightCore, Error, TEXT("Canonical party initialization failed [%s]: %s"),
			*PartyDefinitionId.ToString(), *InitializationResult.Message.ToString());
	}
}

void AKnightPartyGameMode::PreLogin(const FString& Options, const FString& Address,
	const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}
	if (!InitializationResult.IsSuccess() || PartyState == nullptr)
	{
		ErrorMessage = TEXT("KnightPartyNotReady");
		return;
	}
	if (PartyState->GetHumanCount() >= UKnightSettings::CanonicalPartySize)
	{
		ErrorMessage = TEXT("KnightPartyFull: the campaign supports at most three humans.");
		return;
	}
	const FString SlotOption = UGameplayStatics::ParseOption(Options, TEXT("KnightSlot"));
	if (!SlotOption.IsEmpty())
	{
		int32 RequestedSlot = INDEX_NONE;
		if (!LexTryParseString(RequestedSlot, *SlotOption)
			|| RequestedSlot < 0 || RequestedSlot >= UKnightSettings::CanonicalPartySize)
		{
			ErrorMessage = TEXT("KnightSlotInvalid: choose a slot from 0 through 2.");
			return;
		}
		const FKnightRosterEntry* Entry = PartyState->FindSlot(RequestedSlot);
		if (Entry == nullptr || Entry->OwnerKind == EKnightControllerOwner::Human)
		{
			ErrorMessage = TEXT("KnightSlotOccupied: the requested Knight is controlled by another human.");
		}
	}
}

FString AKnightPartyGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	const FString ParentError = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!ParentError.IsEmpty())
	{
		return ParentError;
	}
	int32 DesiredSlot = INDEX_NONE;
	const FString SlotOption = UGameplayStatics::ParseOption(Options, TEXT("KnightSlot"));
	if (!SlotOption.IsEmpty()
		&& (!LexTryParseString(DesiredSlot, *SlotOption) || DesiredSlot < 0
			|| DesiredSlot >= UKnightSettings::CanonicalPartySize))
	{
		return TEXT("KnightSlotInvalid");
	}
	DesiredLoginSlots.Add(NewPlayerController, DesiredSlot);
	return FString();
}

void AKnightPartyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	RestartPlayer(NewPlayer);
}

void AKnightPartyGameMode::RestartPlayer(AController* NewPlayer)
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
	const FKnightResult Result = AssignHumanSlot(Player, DesiredSlot != nullptr ? *DesiredSlot : INDEX_NONE);
	if (!Result.IsSuccess())
	{
		RejectHumanAssignment(Player, Result);
		return;
	}
	DesiredLoginSlots.Remove(Player);
}

void AKnightPartyGameMode::Logout(AController* Exiting)
{
	APlayerController* Player = Cast<APlayerController>(Exiting);
	if (Player != nullptr)
	{
		DesiredLoginSlots.Remove(Player);
	}
	const int32 Slot = PartyState != nullptr ? PartyState->GetControlledSlot(Exiting) : INDEX_NONE;
	const FKnightRosterEntry* Entry = PartyState != nullptr ? PartyState->FindSlot(Slot) : nullptr;
	AKnightCharacter* Knight = Entry != nullptr ? Entry->Knight.Get() : nullptr;
	if (Knight != nullptr && !GetWorld()->bIsTearingDown)
	{
		const FKnightMovementSnapshot Snapshot(Knight);
		Exiting->UnPossess();
		AAIController* Companion = SpawnCompanionController(Knight);
		Snapshot.Restore(Knight);
		if (Companion != nullptr && Companion->GetPawn() == Knight)
		{
			const TArray<int32> Slots { Slot };
			const FKnightResult Result = PartyState->RefreshControllerOwnership(Slots);
			if (!Result.IsSuccess())
			{
				UE_LOG(LogKnightCore, Error, TEXT("AI takeover ownership commit failed slot %d: %s"),
					Slot, *Result.Message.ToString());
			}
			UE_LOG(LogKnightCore, Display, TEXT("AI takeover slot %d Knight %s (canonical count %d)"),
				Slot, *Knight->GetKnightEntityId().Value.ToString(), PartyState->GetRoster().Num());
		}
		else
		{
			UE_LOG(LogKnightCore, Error, TEXT("AI takeover failed slot %d Knight %s; actor preserved"),
				Slot, *Knight->GetKnightEntityId().Value.ToString());
		}
	}
	if (AKnightPlayerState* Human = Exiting != nullptr ? Exiting->GetPlayerState<AKnightPlayerState>() : nullptr)
	{
		Human->SetAssignedSlot(INDEX_NONE);
	}
	Super::Logout(Exiting);
}

AKnightPartyState* AKnightPartyGameMode::GetPartyState() const
{
	return PartyState;
}

const FKnightResult& AKnightPartyGameMode::GetInitializationResult() const
{
	return InitializationResult;
}

FVector AKnightPartyGameMode::GetSpawnLocationForSlot(int32 Slot) const
{
	return SpawnOrigin + FVector(0.0, static_cast<double>(SlotSpacing) * (Slot - 1), 0.0);
}

FKnightResult AKnightPartyGameMode::TrySwitchKnight(APlayerController* Player, int32 DesiredSlot)
{
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("SwitchKnight"),
			LOCTEXT("SwitchAuthority", "Only the server may transfer Knight control."));
	}
	if (!IsValid(Player) || Player->GetWorld() != GetWorld() || PartyState == nullptr
		|| !PartyState->IsCanonicalRosterReady())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("SwitchKnight"),
			LOCTEXT("SwitchNotReady", "The human controller and canonical party must be ready before switching."));
	}
	if (PartyState->GetHumanCount() != 1)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("SwitchKnight"),
			LOCTEXT("SwitchSoloOnly", "Knight switching is available when exactly one human controls the party."));
	}
	const int32 CurrentSlot = PartyState->GetControlledSlot(Player);
	const FKnightRosterEntry* PreviousEntry = PartyState->FindSlot(CurrentSlot);
	if (PreviousEntry == nullptr || PreviousEntry->OwnerKind != EKnightControllerOwner::Human
		|| PreviousEntry->Knight != Player->GetPawn())
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("SwitchKnight"),
			LOCTEXT("SwitchOwnership", "The caller does not own a canonical Knight."));
	}
	if (DesiredSlot == INDEX_NONE)
	{
		DesiredSlot = (CurrentSlot + 1) % UKnightSettings::CanonicalPartySize;
	}
	if (DesiredSlot < 0 || DesiredSlot >= UKnightSettings::CanonicalPartySize)
	{
		return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("SwitchKnight"),
			LOCTEXT("SwitchSlotRange", "The requested Knight slot does not exist."));
	}
	if (DesiredSlot == CurrentSlot)
	{
		return FKnightResult::Success();
	}
	const FKnightRosterEntry* TargetEntry = PartyState->FindSlot(DesiredSlot);
	AKnightCharacter* PreviousKnight = PreviousEntry->Knight;
	AKnightCharacter* TargetKnight = TargetEntry != nullptr ? TargetEntry->Knight.Get() : nullptr;
	AAIController* Companion = TargetKnight != nullptr ? Cast<AAIController>(TargetKnight->GetController()) : nullptr;
	if (TargetEntry == nullptr || TargetEntry->OwnerKind != EKnightControllerOwner::Companion
		|| !IsValid(Companion) || Companion->GetPawn() != TargetKnight)
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("SwitchKnight"),
			LOCTEXT("SwitchTargetOwned", "The target Knight is not available for human takeover."));
	}
	if (!PreviousKnight->CanTransferControl() || !TargetKnight->CanTransferControl())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("SwitchKnight"),
			LOCTEXT("SwitchStateBlocked", "A Knight's current gameplay state blocks control transfer."));
	}
	AKnightPlayerState* Human = Player->GetPlayerState<AKnightPlayerState>();
	if (Human == nullptr || !Human->GetHumanId().IsValid())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("SwitchKnight"),
			LOCTEXT("SwitchHumanMissing", "The human connection identity is unavailable."));
	}

	const FKnightMovementSnapshot PreviousSnapshot(PreviousKnight);
	const FKnightMovementSnapshot TargetSnapshot(TargetKnight);
	PreviousKnight->ResetControlInput();
	TargetKnight->ResetControlInput();
	Companion->UnPossess();
	Player->UnPossess();
	Player->Possess(TargetKnight);
	Companion->Possess(PreviousKnight);
	const bool bControlTransferred = Player->GetPawn() == TargetKnight && TargetKnight->GetController() == Player
		&& Companion->GetPawn() == PreviousKnight && PreviousKnight->GetController() == Companion;
	PreviousSnapshot.Restore(PreviousKnight);
	TargetSnapshot.Restore(TargetKnight);
	if (!bControlTransferred)
	{
		const bool bRolledBack = RestoreControl(Player, PreviousKnight, TargetKnight, Companion);
		PreviousSnapshot.Restore(PreviousKnight);
		TargetSnapshot.Restore(TargetKnight);
		UE_LOG(LogKnightCore, Error, TEXT("Knight transfer failed; rollback %s"), bRolledBack ? TEXT("succeeded") : TEXT("failed"));
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("SwitchKnight"),
			LOCTEXT("SwitchPossessionFailed", "Controller transfer failed; the previous ownership was restored where possible."));
	}
	const TArray<int32> ChangedSlots { CurrentSlot, DesiredSlot };
	const FKnightResult Commit = PartyState->RefreshControllerOwnership(ChangedSlots);
	if (!Commit.IsSuccess())
	{
		const bool bRolledBack = RestoreControl(Player, PreviousKnight, TargetKnight, Companion);
		PreviousSnapshot.Restore(PreviousKnight);
		TargetSnapshot.Restore(TargetKnight);
		UE_LOG(LogKnightCore, Error, TEXT("Knight ownership commit failed; rollback %s: %s"),
			bRolledBack ? TEXT("succeeded") : TEXT("failed"), *Commit.Message.ToString());
		return Commit;
	}
	Human->SetAssignedSlot(DesiredSlot);
	if (AKnightGameState* State = GetGameState<AKnightGameState>())
	{
		const FKnightResult Progress = State->RecordControlledSlot(Player, DesiredSlot);
		if (!Progress.IsSuccess())
		{
			UE_LOG(LogKnightCore, Error, TEXT("Transferred Knight training record failed: %s"), *Progress.Message.ToString());
		}
	}
	UE_LOG(LogKnightCore, Display, TEXT("Knight transfer %d -> %d; IDs/actors preserved; canonical count %d"),
		CurrentSlot, DesiredSlot, PartyState->GetRoster().Num());
	return FKnightResult::Success();
}

FKnightResult AKnightPartyGameMode::TryRestartDrill(APlayerController* Player)
{
	if (AKnightGameState* State = GetGameState<AKnightGameState>())
	{
		return State->ResetTrainingProgress(Player);
	}
	return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RestartDrill"),
		LOCTEXT("RestartStateMissing", "The training state is unavailable."));
}

FKnightResult AKnightPartyGameMode::TryInteract(APlayerController* Player)
{
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("Interact"),
			LOCTEXT("InteractAuthority", "Only the server may resolve an interaction."));
	}
	if (!IsValid(Player) || Player->GetWorld() != GetWorld() || PartyState == nullptr
		|| PartyState->GetControlledSlot(Player) == INDEX_NONE)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("Interact"),
			LOCTEXT("InteractOwnership", "Interaction requires an assigned human controlling a canonical Knight."));
	}
	return FKnightResult::Failure(EKnightError::InvalidState, TEXT("Interact"),
		LOCTEXT("InteractUnavailable", "This training mode does not contain interaction stations."));
}

FKnightResult AKnightPartyGameMode::TryRecoverKnightFromFall(AKnightCharacter* Knight)
{
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("RecoverKnight"),
			LOCTEXT("RecoveryAuthority", "Only the server may recover a fallen canonical Knight."));
	}
	UWorld* World = GetWorld();
	if (World == nullptr || World->bIsTearingDown || World->IsInSeamlessTravel())
	{
		return FKnightResult::Failure(EKnightError::ShuttingDown, TEXT("RecoverKnight"),
			LOCTEXT("RecoveryWorldUnavailable", "Knight recovery is unavailable while the world is changing."));
	}
	if (!IsValid(Knight) || Knight->IsActorBeingDestroyed()
		|| Knight->GetWorld() != World || !Knight->HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("RecoverKnight"),
			LOCTEXT("RecoveryWrongWorld", "Recovery requires a canonical Knight from this authoritative world."));
	}
	const FKnightRosterEntry* Entry = PartyState != nullptr ? PartyState->FindKnight(Knight) : nullptr;
	if (Entry == nullptr || Entry->Slot < 0 || Entry->Slot >= UKnightSettings::CanonicalPartySize
		|| Entry->KnightId != Knight->GetKnightEntityId()
		|| Entry->HeroDefinitionId != Knight->GetHeroDefinitionId()
		|| PartyState->FindSlot(Entry->Slot) != Entry)
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("RecoverKnight"),
			LOCTEXT("RecoveryNotCanonical", "The fallen actor does not match a canonical roster identity and slot."));
	}
	UCharacterMovementComponent* Movement = Knight->GetCharacterMovement();
	if (!PartyState->IsCanonicalRosterReady() || Movement == nullptr)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RecoverKnight"),
			LOCTEXT("RecoveryNotReady", "The existing canonical party and movement component must be ready."));
	}
	const FVector RecoveryLocation = GetSpawnLocationForSlot(Entry->Slot);
	if (RecoveryLocation.ContainsNaN())
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("RecoverKnight"),
			LOCTEXT("RecoveryInvalidLocation", "The configured recovery location is invalid."));
	}
	if (AAIController* Companion = Cast<AAIController>(Knight->GetController()))
	{
		Companion->StopMovement();
	}
	APlayerController* Player = Cast<APlayerController>(Knight->GetController());
	if (AKnightPlayerController* Human = Cast<AKnightPlayerController>(Player))
	{
		Human->ResetControlInput();
	}
	else if (Player != nullptr)
	{
		Player->FlushPressedKeys();
	}
	Knight->GetCombatComponent()->ClearTransientActions();
	Knight->ResetControlInput();
	Movement->StopMovementImmediately();
	if (!Knight->TeleportTo(RecoveryLocation, FRotator::ZeroRotator, false, false))
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("RecoverKnight"),
			LOCTEXT("RecoveryTeleportBlocked", "Collision prevented recovery; the canonical actor was preserved."));
	}
	if (Player != nullptr)
	{
		Player->SetControlRotation(FRotator::ZeroRotator);
		Player->ClientSetRotation(FRotator::ZeroRotator);
	}
	Movement->SetMovementMode(Knight->GetLifeComponent()->IsAlive() ? MOVE_Walking : MOVE_None);
	Knight->ForceNetUpdate();
	UE_LOG(LogKnightCore, Display, TEXT("Recovered fallen Knight %s in slot %d; actor/identity/controller preserved"),
		*Entry->KnightId.Value.ToString(), Entry->Slot);
	return FKnightResult::Success();
}

void AKnightPartyGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DesiredLoginSlots.Reset();
	Super::EndPlay(EndPlayReason);
}

FKnightResult AKnightPartyGameMode::EnsureCanonicalParty()
{
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("SpawnParty"),
			LOCTEXT("SpawnAuthority", "Clients cannot spawn the canonical party."));
	}
	if (PartyState != nullptr)
	{
		return PartyState->IsCanonicalRosterReady() ? FKnightResult::Success()
			: FKnightResult::Failure(EKnightError::InvalidState, TEXT("SpawnParty"),
				LOCTEXT("ExistingPartyInvalid", "The existing canonical party is invalid and cannot be silently rebuilt."));
	}
	AKnightGameState* State = GetGameState<AKnightGameState>();
	UKnightAssetManager* Manager = UKnightAssetManager::GetKnightAssetManager();
	if (State == nullptr || Manager == nullptr || KnightCharacterClass == nullptr || AIControllerClass == nullptr)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("SpawnParty"),
			LOCTEXT("CompositionMissing", "Party spawning requires configured framework, character, AI and asset manager classes."));
	}
	UKnightDefinition* Definition = nullptr;
	const FKnightResult PartyLoad = Manager->ResolveDefinition(PartyDefinitionId, UKnightPartyDefinition::StaticClass(), Definition);
	if (!PartyLoad.IsSuccess())
	{
		return PartyLoad;
	}
	UKnightPartyDefinition* PartyDefinition = CastChecked<UKnightPartyDefinition>(Definition);
	const FKnightResult PartyValidation = PartyDefinition->ValidateParty();
	if (!PartyValidation.IsSuccess())
	{
		return PartyValidation;
	}
	TArray<UKnightHeroDefinition*> Heroes;
	for (const FPrimaryAssetId& HeroId : PartyDefinition->GetHeroDefinitions())
	{
		const FKnightResult HeroLoad = Manager->ResolveDefinition(HeroId, UKnightHeroDefinition::StaticClass(), Definition);
		if (!HeroLoad.IsSuccess())
		{
			return HeroLoad;
		}
		UKnightHeroDefinition* Hero = CastChecked<UKnightHeroDefinition>(Definition);
		const FKnightResult HeroValidation = Hero->ValidateHero();
		if (!HeroValidation.IsSuccess())
		{
			return HeroValidation;
		}
		Heroes.Add(Hero);
	}

	FActorSpawnParameters PartyParameters;
	PartyParameters.Owner = this;
	AKnightPartyState* NewParty = GetWorld()->SpawnActor<AKnightPartyState>(PartyParameters);
	if (NewParty == nullptr)
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("SpawnParty"),
			LOCTEXT("PartyActorSpawnFailed", "The canonical PartyState actor could not be spawned."));
	}
	TArray<AKnightCharacter*> SpawnedKnights;
	TArray<AAIController*> SpawnedCompanions;
	TArray<FKnightRosterEntry> Entries;
	FKnightResult SpawnResult = FKnightResult::Success();
	for (int32 Slot = 0; Slot < UKnightSettings::CanonicalPartySize; ++Slot)
	{
		const FTransform Transform(FRotator::ZeroRotator, GetSpawnLocationForSlot(Slot));
		AKnightCharacter* Knight = GetWorld()->SpawnActorDeferred<AKnightCharacter>(KnightCharacterClass,
			Transform, NewParty, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (Knight == nullptr)
		{
			SpawnResult = FKnightResult::Failure(EKnightError::InvalidState, TEXT("SpawnParty"),
				LOCTEXT("KnightSpawnFailed", "A canonical Knight could not be spawned."));
			break;
		}
		SpawnedKnights.Add(Knight);
		const FKnightEntityId KnightId = FKnightEntityId::Create();
		SpawnResult = Knight->InitializeKnight(KnightId, Heroes[Slot]);
		if (!SpawnResult.IsSuccess())
		{
			break;
		}
		Knight->FinishSpawning(Transform);
		AAIController* Companion = SpawnCompanionController(Knight);
		if (Companion == nullptr || Companion->GetPawn() != Knight)
		{
			SpawnResult = FKnightResult::Failure(EKnightError::InvalidState, TEXT("SpawnParty"),
				LOCTEXT("CompanionSpawnFailed", "A canonical Knight could not receive its companion controller."));
			break;
		}
		SpawnedCompanions.Add(Companion);
		FKnightRosterEntry Entry;
		Entry.Slot = Slot;
		Entry.KnightId = KnightId;
		Entry.HeroDefinitionId = Heroes[Slot]->GetPrimaryAssetId();
		Entry.Knight = Knight;
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
		for (AKnightCharacter* Knight : SpawnedKnights)
		{
			Knight->Destroy();
		}
		NewParty->Destroy();
		return SpawnResult;
	}
	PartyState = NewParty;
	UE_LOG(LogKnightCore, Display, TEXT("Canonical party created: %s, three Knight actors, three companion controllers"),
		*PartyDefinitionId.ToString());
	return FKnightResult::Success();
}

FKnightResult AKnightPartyGameMode::AssignHumanSlot(APlayerController* Player, int32 DesiredSlot)
{
	if (!HasAuthority())
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("AssignHuman"),
			LOCTEXT("AssignAuthority", "Only the server may assign human Knight ownership."));
	}
	if (!IsValid(Player) || Player->GetWorld() != GetWorld() || PartyState == nullptr
		|| !PartyState->IsCanonicalRosterReady())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("AssignHuman"),
			LOCTEXT("AssignNotReady", "The human and canonical party must be ready before assignment."));
	}
	if (PartyState->GetControlledSlot(Player) != INDEX_NONE)
	{
		return FKnightResult::Success();
	}
	if (PartyState->GetHumanCount() >= UKnightSettings::CanonicalPartySize)
	{
		return FKnightResult::Failure(EKnightError::CapacityExceeded, TEXT("AssignHuman"),
			LOCTEXT("AssignPartyFull", "The campaign already has three human players."));
	}
	if (DesiredSlot == INDEX_NONE)
	{
		for (const FKnightRosterEntry& Candidate : PartyState->GetRoster())
		{
			if (Candidate.OwnerKind == EKnightControllerOwner::Companion)
			{
				DesiredSlot = Candidate.Slot;
				break;
			}
		}
	}
	const FKnightRosterEntry* Entry = PartyState->FindSlot(DesiredSlot);
	if (Entry == nullptr)
	{
		return FKnightResult::Failure(EKnightError::OutOfRange, TEXT("AssignHuman"),
			LOCTEXT("AssignSlotInvalid", "The requested party slot does not exist."));
	}
	if (Entry->OwnerKind != EKnightControllerOwner::Companion)
	{
		return FKnightResult::Failure(EKnightError::Conflict, TEXT("AssignHuman"),
			LOCTEXT("AssignSlotOccupied", "The requested Knight is already controlled by another human."));
	}
	AKnightPlayerState* Human = Player->GetPlayerState<AKnightPlayerState>();
	AKnightCharacter* Knight = Entry->Knight;
	AAIController* PreviousCompanion = IsValid(Knight) ? Cast<AAIController>(Knight->GetController()) : nullptr;
	if (Human == nullptr || PreviousCompanion == nullptr || !IsValid(Knight)
		|| Knight->GetWorld() != GetWorld() || Knight->IsActorBeingDestroyed()
		|| !Knight->GetKnightEntityId().IsValid() || Knight->GetCharacterMovement() == nullptr
		|| GetWorld()->bIsTearingDown || GetWorld()->IsInSeamlessTravel())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("AssignHuman"),
			LOCTEXT("AssignControllerMissing", "The human identity, existing Knight or companion controller is unavailable."));
	}
	const FKnightResult Identity = Human->InitializeHumanIdentity();
	if (!Identity.IsSuccess())
	{
		return Identity;
	}
	const FKnightMovementSnapshot Snapshot(Knight);
	PreviousCompanion->UnPossess();
	Player->Possess(Knight);
	Snapshot.Restore(Knight);
	if (Player->GetPawn() != Knight || Knight->GetController() != Player)
	{
		Player->UnPossess();
		PreviousCompanion->Possess(Knight);
		Snapshot.Restore(Knight);
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("AssignHuman"),
			LOCTEXT("AssignPossessionFailed", "Human possession failed; companion ownership was restored."));
	}
	const TArray<int32> Slots { DesiredSlot };
	const FKnightResult Commit = PartyState->RefreshControllerOwnership(Slots);
	if (!Commit.IsSuccess())
	{
		Player->UnPossess();
		PreviousCompanion->Possess(Knight);
		Snapshot.Restore(Knight);
		return Commit;
	}
	Human->SetAssignedSlot(DesiredSlot);
	PreviousCompanion->Destroy();
	if (AKnightGameState* State = GetGameState<AKnightGameState>())
	{
		const FKnightResult Progress = State->RecordControlledSlot(Player, DesiredSlot);
		if (!Progress.IsSuccess())
		{
			UE_LOG(LogKnightCore, Error, TEXT("Assigned Knight training record failed: %s"), *Progress.Message.ToString());
		}
	}
	UE_LOG(LogKnightCore, Display, TEXT("Human %d assigned Knight slot %d; %d humans, canonical count %d"),
		Human->GetPlayerId(), DesiredSlot, PartyState->GetHumanCount(), PartyState->GetRoster().Num());
	return FKnightResult::Success();
}

AAIController* AKnightPartyGameMode::SpawnCompanionController(AKnightCharacter* Knight)
{
	if (!HasAuthority() || !IsValid(Knight) || AIControllerClass == nullptr)
	{
		return nullptr;
	}
	if (AAIController* Existing = Cast<AAIController>(Knight->GetController()))
	{
		return Existing;
	}
	FActorSpawnParameters Parameters;
	Parameters.Owner = this;
	AAIController* Companion = GetWorld()->SpawnActor<AAIController>(AIControllerClass,
		FVector::ZeroVector, FRotator::ZeroRotator, Parameters);
	if (Companion != nullptr)
	{
		Companion->Possess(Knight);
		if (Companion->GetPawn() != Knight)
		{
			Companion->Destroy();
			return nullptr;
		}
	}
	return Companion;
}

void AKnightPartyGameMode::RejectHumanAssignment(APlayerController* Player, const FKnightResult& Result)
{
	UE_LOG(LogKnightCore, Error, TEXT("Human assignment rejected [%s]: %s"),
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
