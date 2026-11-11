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

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"

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
		const TArray<FKey> Keys{EKeys::N, EKeys::Z, EKeys::B, EKeys::K, EKeys::Gamepad_Special_Left};
		for (int32 Index = 0; Index < Keys.Num(); ++Index)
		{
			UInputAction* Action = NewObject<UInputAction>(this);
			Action->ValueType = EInputActionValueType::Boolean;
			CommandActions.Add(Action);
			CommandMappings->MapKey(Action, Keys[Index]);
		}
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
		AActor* Target = Golem->GetCombatComponent()->GetLockedTarget();
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
		Focus = Golem->GetCombatComponent()->GetLockedTarget();
		UGolemAbilitySystemComponent* FocusASC = Focus != nullptr
			? Focus->FindComponentByClass<UGolemAbilitySystemComponent>() : nullptr;
		if (!FocusId.IsValid() || FocusASC == nullptr || !(FocusASC->GetEntityId() == FocusId))
		{
			return FGolemResult::Failure(EGolemError::InvalidId, TEXT("CompanionFocus"),
				NSLOCTEXT("GolemArena", "FocusLock", "Lock a living enemy with MMB before issuing Focus."));
		}
		const FGolemResult Target = Golem->GetCombatComponent()->ValidateCombatTarget(Focus, 2200.0f);
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
