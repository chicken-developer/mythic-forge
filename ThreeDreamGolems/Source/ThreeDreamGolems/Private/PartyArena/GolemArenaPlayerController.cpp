#include "PartyArena/GolemArenaPlayerController.h"

#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Context/GolemAIContextComponent.h"
#include "Controllers/GolemArenaCompanionController.h"
#include "Framework/GolemGameState.h"
#include "Party/GolemPartyState.h"
#include "Player/GolemPlayerState.h"
#include "Voids/GolemColorVoidComponent.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"

namespace
{
	/** Focus reaches further than an attack does: the companions walk to the enemy that was pointed out. */
	constexpr float GolemArenaFocusRange = 2200.0f;

	/**
	 * The living enemy of Source that carries this stable id, or nullptr: an id that names a friend or
	 * nobody is not a focus target. The caller still validates range and line of sight.
	 */
	AActor* GolemArenaFindFocusEnemy(const AActor* Source, const FGolemEntityId& FocusId)
	{
		const UWorld* World = Source != nullptr ? Source->GetWorld() : nullptr;
		if (World == nullptr || !FocusId.IsValid())
		{
			return nullptr;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const UGolemAbilitySystemComponent* CandidateASC = It->FindComponentByClass<UGolemAbilitySystemComponent>();
			if (CandidateASC != nullptr && CandidateASC->GetEntityId() == FocusId)
			{
				return UGolemCombatComponent::AreHostile(Source, *It) ? *It : nullptr;
			}
		}
		return nullptr;
	}
}

void AGolemArenaPlayerController::SetPlayer(UPlayer* InPlayer)
{
	RemoveCommandMappings();
	Super::SetPlayer(InPlayer);
	InstallCommandMappings();
}

void AGolemArenaPlayerController::BeginPlay()
{
	Super::BeginPlay();
	InstallCommandMappings();
}

void AGolemArenaPlayerController::InstallCommandMappings()
{
	if (CommandMappings == nullptr)
	{
		CommandMappings = NewObject<UInputMappingContext>(this, TEXT("ArenaCommands"));
		const TArray<FKey> Keys{EKeys::N, EKeys::Z, EKeys::B, EKeys::K, EKeys::Gamepad_Special_Left, EKeys::X};
		for (int32 Index = 0; Index < Keys.Num(); ++Index)
		{
			UInputAction* Action = NewObject<UInputAction>(this);
			Action->ValueType = EInputActionValueType::Boolean;
			CommandActions.Add(Action);
			CommandMappings->MapKey(Action, Keys[Index]);
		}
		// Signal: X on the keyboard, D-pad Up on a gamepad.
		CommandMappings->MapKey(CommandActions[5], EKeys::Gamepad_DPad_Up);
	}
	if (IsLocalController() && GetLocalPlayer() != nullptr)
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		if (Subsystem != nullptr && MappingSubsystem.Get() != Subsystem)
		{
			RemoveCommandMappings();
			Subsystem->AddMappingContext(CommandMappings, 20);
			MappingSubsystem = Subsystem;
		}
	}
}

void AGolemArenaPlayerController::RemoveCommandMappings()
{
	if (MappingSubsystem.IsValid() && CommandMappings != nullptr)
	{
		MappingSubsystem->RemoveMappingContext(CommandMappings);
	}
	MappingSubsystem.Reset();
}

void AGolemArenaPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InstallCommandMappings();
	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
	if (Enhanced == nullptr || (BoundCommandInput.Get() == Enhanced && !BindingHandles.IsEmpty()))
	{
		return;
	}
	if (BoundCommandInput.IsValid())
	{
		for (const uint32 Handle : BindingHandles)
		{
			BoundCommandInput->RemoveBindingByHandle(Handle);
		}
	}
	BindingHandles.Reset();
	BoundCommandInput = Enhanced;
	const TArray<EGolemCompanionCommand> Commands{EGolemCompanionCommand::Follow,
		EGolemCompanionCommand::Hold, EGolemCompanionCommand::Focus, EGolemCompanionCommand::Regroup};
	for (int32 Index = 0; Index < Commands.Num(); ++Index)
	{
		BindingHandles.Add(Enhanced->BindAction(CommandActions[Index], ETriggerEvent::Started,
			this, &ThisClass::HandleCommand, Commands[Index]).GetHandle());
	}
	BindingHandles.Add(Enhanced->BindAction(CommandActions[4], ETriggerEvent::Started,
		this, &ThisClass::HandleCycleCommand).GetHandle());
	BindingHandles.Add(Enhanced->BindAction(CommandActions[5], ETriggerEvent::Started,
		this, &ThisClass::RequestSignal).GetHandle());
}

void AGolemArenaPlayerController::RequestSignal()
{
	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);
	UGolemColorVoidComponent* Void = UGolemColorVoidComponent::FindAimedVoid(GetWorld(), ViewLocation,
		ViewRotation.Vector(), SignalRange + 1500.0f, SignalAimDegrees);
	if (Void == nullptr)
	{
		LastSignalResult = FGolemResult::Failure(EGolemError::NotFound, TEXT("SignalAim"),
			NSLOCTEXT("GolemArena", "SignalNoVoid", "Point the camera at a glowing void, then signal."));
		OnCompanionCommandResult.Broadcast(LastSignalResult);
		return;
	}
	ServerSignalVoid(Void);
}

FGolemResult AGolemArenaPlayerController::ApplySignal(UGolemColorVoidComponent* Void)
{
	if (!HasAuthority())
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("Signal"), FText::GetEmpty());
	}
	const AGolemCharacter* Golem = GetControlledGolem();
	if (Golem == nullptr || !Golem->GetLifeComponent()->IsAlive())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("SignalGolem"),
			NSLOCTEXT("GolemArena", "SignalDowned", "Only a standing Golem can signal."));
	}
	if (!IsValid(Void) || !IsValid(Void->GetOwner()) || Void->GetWorld() != GetWorld() || !Void->AcceptsHits())
	{
		return FGolemResult::Failure(EGolemError::NotFound, TEXT("SignalVoid"),
			NSLOCTEXT("GolemArena", "SignalClosed", "That void cannot take colour right now."));
	}
	if (FVector::DistSquared(Golem->GetActorLocation(), Void->GetComponentLocation()) > FMath::Square(SignalRange))
	{
		return FGolemResult::Failure(EGolemError::OutOfRange, TEXT("SignalRange"),
			NSLOCTEXT("GolemArena", "SignalFar", "Too far away to signal that void."));
	}
	return Void->Signal(const_cast<AGolemCharacter*>(Golem)) ? FGolemResult::Success()
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("SignalVoid"), FText::GetEmpty());
}

void AGolemArenaPlayerController::ServerSignalVoid_Implementation(UGolemColorVoidComponent* Void)
{
	ClientReceiveSignalResult(ApplySignal(Void));
}

void AGolemArenaPlayerController::ClientReceiveSignalResult_Implementation(const FGolemResult Result)
{
	LastSignalResult = Result;
	if (!Result.IsSuccess())
	{
		// Failures reuse the command feedback line; a successful signal shows on the void itself.
		OnCompanionCommandResult.Broadcast(Result);
	}
}

void AGolemArenaPlayerController::HandleCommand(const EGolemCompanionCommand Command)
{
	RequestCompanionCommand(Command);
}

void AGolemArenaPlayerController::HandleCycleCommand()
{
	const uint8 Next = (static_cast<uint8>(LastCompanionCommand) + 1) % 4;
	RequestCompanionCommand(static_cast<EGolemCompanionCommand>(Next));
}

void AGolemArenaPlayerController::RequestCompanionCommand(const EGolemCompanionCommand Command)
{
	AGolemCharacter* Golem = GetControlledGolem();
	FGolemEntityId FocusId;
	if (Golem != nullptr && Command == EGolemCompanionCommand::Focus)
	{
		// Focus points the companions at the nearest enemy this Golem can see.
		AActor* Target = Golem->GetCombatComponent()->FindCombatTarget(GolemArenaFocusRange);
		UGolemAbilitySystemComponent* TargetASC = Target != nullptr
			? Target->FindComponentByClass<UGolemAbilitySystemComponent>() : nullptr;
		if (TargetASC != nullptr)
		{
			FocusId = TargetASC->GetEntityId();
		}
	}
	ServerRequestCompanionCommand(Command, Golem != nullptr ? Golem->GetGolemEntityId() : FGolemEntityId(), FocusId);
}

FGolemResult AGolemArenaPlayerController::ApplyCompanionCommand(const EGolemCompanionCommand Command,
	const FGolemEntityId& ExpectedGolemId, const FGolemEntityId& FocusId)
{
	if (!HasAuthority() || static_cast<uint8>(Command) > static_cast<uint8>(EGolemCompanionCommand::Regroup))
	{
		return FGolemResult::Failure(EGolemError::NotAuthority, TEXT("CompanionCommand"), FText::GetEmpty());
	}
	AGolemCharacter* Golem = GetControlledGolem();
	AGolemGameState* State = GetWorld() != nullptr ? GetWorld()->GetGameState<AGolemGameState>() : nullptr;
	AGolemPartyState* Party = State != nullptr ? State->GetPartyState() : nullptr;
	AGolemPlayerState* Human = GetPlayerState<AGolemPlayerState>();
	const int32 Slot = Party != nullptr ? Party->GetControlledSlot(this) : INDEX_NONE;
	const FGolemRosterEntry* Owned = Party != nullptr ? Party->FindSlot(Slot) : nullptr;
	if (Golem == nullptr || Owned == nullptr || Human == nullptr || !Human->GetHumanId().IsValid()
		|| Human->GetAssignedSlot() != Slot || Owned->HumanPlayerState != Human || Owned->Golem != Golem
		|| Owned->OwnerKind != EGolemControllerOwner::Human || Owned->HumanId != Human->GetHumanId()
		|| !ExpectedGolemId.IsValid() || !(ExpectedGolemId == Golem->GetGolemEntityId())
		|| !Golem->GetLifeComponent()->IsAlive())
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("CompanionCommand"),
			NSLOCTEXT("GolemArena", "CommandOwner", "A companion command requires your current living canonical Golem."));
	}
	AActor* Focus = nullptr;
	if (Command == EGolemCompanionCommand::Focus)
	{
		Focus = GolemArenaFindFocusEnemy(Golem, FocusId);
		if (Focus == nullptr)
		{
			return FGolemResult::Failure(EGolemError::InvalidId, TEXT("CompanionFocus"),
				NSLOCTEXT("GolemArena", "FocusTarget", "Focus needs a living enemy in sight."));
		}
		const FGolemResult Target = Golem->GetCombatComponent()->ValidateCombatTarget(Focus, GolemArenaFocusRange);
		if (!Target.IsSuccess())
		{
			return Target;
		}
	}
	else if (FocusId.IsValid())
	{
		return FGolemResult::Failure(EGolemError::InvalidData, TEXT("CompanionCommand"), FText::GetEmpty());
	}
	TArray<AGolemArenaCompanionController*> Candidates;
	for (const FGolemRosterEntry& Entry : Party->GetRoster())
	{
		AGolemArenaCompanionController* Companion = Cast<AGolemArenaCompanionController>(Entry.Golem->GetController());
		if (Companion == nullptr || Entry.OwnerKind != EGolemControllerOwner::Companion
			|| !Entry.Golem->GetLifeComponent()->IsAlive())
		{
			continue;
		}
		AGolemCharacter* NearestHuman = nullptr;
		double NearestDistance = TNumericLimits<double>::Max();
		for (const FGolemRosterEntry& HumanEntry : Party->GetRoster())
		{
			if (HumanEntry.OwnerKind == EGolemControllerOwner::Human && HumanEntry.Golem->GetLifeComponent()->IsAlive())
			{
				const double Distance = FVector::DistSquared(Entry.Golem->GetActorLocation(),
					HumanEntry.Golem->GetActorLocation());
				if (Distance < NearestDistance)
				{
					NearestHuman = HumanEntry.Golem;
					NearestDistance = Distance;
				}
			}
		}
		if (NearestHuman == Golem && Companion->GetArenaInitializationResult().IsSuccess()
			&& Companion->GetAIContext()->GetControlledPawn() == Entry.Golem)
		{
			Candidates.Add(Companion);
		}
	}
	if (Candidates.IsEmpty())
	{
		return FGolemResult::Failure(EGolemError::NotFound, TEXT("CompanionCommand"),
			NSLOCTEXT("GolemArena", "NoCompanions", "No living AI companion currently follows your Golem."));
	}
	for (AGolemArenaCompanionController* Companion : Candidates)
	{
		const FGolemResult Result = Companion->TrySetCommand(Command, Focus);
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	return FGolemResult::Success();
}

void AGolemArenaPlayerController::ServerRequestCompanionCommand_Implementation(
	const EGolemCompanionCommand Command, const FGolemEntityId ExpectedGolemId, const FGolemEntityId FocusId)
{
	const FGolemResult Result = ApplyCompanionCommand(Command, ExpectedGolemId, FocusId);
	ClientReceiveCompanionCommandResult(Command, Result);
}

void AGolemArenaPlayerController::ClientReceiveCompanionCommandResult_Implementation(
	const EGolemCompanionCommand Command, const FGolemResult Result)
{
	LastCompanionCommandResult = Result;
	if (Result.IsSuccess())
	{
		LastCompanionCommand = Command;
	}
	OnCompanionCommandResult.Broadcast(Result);
}

const FGolemResult& AGolemArenaPlayerController::GetLastCompanionCommandResult() const
{
	return LastCompanionCommandResult;
}

EGolemCompanionCommand AGolemArenaPlayerController::GetLastCompanionCommand() const
{
	return LastCompanionCommand;
}

void AGolemArenaPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveCommandMappings();
	if (BoundCommandInput.IsValid())
	{
		for (const uint32 Handle : BindingHandles)
		{
			BoundCommandInput->RemoveBindingByHandle(Handle);
		}
	}
	BindingHandles.Reset();
	BoundCommandInput.Reset();
	Super::EndPlay(EndPlayReason);
}
