#include "PartyArena/KnightArenaPlayerController.h"

#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Context/KnightAIContextComponent.h"
#include "Controllers/KnightArenaCompanionController.h"
#include "Framework/KnightGameState.h"
#include "Party/KnightPartyState.h"
#include "Player/KnightPlayerState.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"

void AKnightArenaPlayerController::SetPlayer(UPlayer* InPlayer)
{
	RemoveCommandMappings();
	Super::SetPlayer(InPlayer);
	InstallCommandMappings();
}

void AKnightArenaPlayerController::BeginPlay()
{
	Super::BeginPlay();
	InstallCommandMappings();
}

void AKnightArenaPlayerController::InstallCommandMappings()
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

void AKnightArenaPlayerController::RemoveCommandMappings()
{
	if (MappingSubsystem.IsValid() && CommandMappings != nullptr)
	{
		MappingSubsystem->RemoveMappingContext(CommandMappings);
	}
	MappingSubsystem.Reset();
}

void AKnightArenaPlayerController::SetupInputComponent()
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
	const TArray<EKnightCompanionCommand> Commands{EKnightCompanionCommand::Follow,
		EKnightCompanionCommand::Hold, EKnightCompanionCommand::Focus, EKnightCompanionCommand::Regroup};
	for (int32 Index = 0; Index < Commands.Num(); ++Index)
	{
		BindingHandles.Add(Enhanced->BindAction(CommandActions[Index], ETriggerEvent::Started,
			this, &ThisClass::HandleCommand, Commands[Index]).GetHandle());
	}
	BindingHandles.Add(Enhanced->BindAction(CommandActions[4], ETriggerEvent::Started,
		this, &ThisClass::HandleCycleCommand).GetHandle());
}

void AKnightArenaPlayerController::HandleCommand(const EKnightCompanionCommand Command)
{
	RequestCompanionCommand(Command);
}

void AKnightArenaPlayerController::HandleCycleCommand()
{
	const uint8 Next = (static_cast<uint8>(LastCompanionCommand) + 1) % 4;
	RequestCompanionCommand(static_cast<EKnightCompanionCommand>(Next));
}

void AKnightArenaPlayerController::RequestCompanionCommand(const EKnightCompanionCommand Command)
{
	AKnightCharacter* Knight = GetControlledKnight();
	FKnightEntityId FocusId;
	if (Knight != nullptr && Command == EKnightCompanionCommand::Focus)
	{
		AActor* Target = Knight->GetCombatComponent()->GetLockedTarget();
		UKnightAbilitySystemComponent* TargetASC = Target != nullptr
			? Target->FindComponentByClass<UKnightAbilitySystemComponent>() : nullptr;
		if (TargetASC != nullptr)
		{
			FocusId = TargetASC->GetEntityId();
		}
	}
	ServerRequestCompanionCommand(Command, Knight != nullptr ? Knight->GetKnightEntityId() : FKnightEntityId(), FocusId);
}

FKnightResult AKnightArenaPlayerController::ApplyCompanionCommand(const EKnightCompanionCommand Command,
	const FKnightEntityId& ExpectedKnightId, const FKnightEntityId& FocusId)
{
	if (!HasAuthority() || static_cast<uint8>(Command) > static_cast<uint8>(EKnightCompanionCommand::Regroup))
	{
		return FKnightResult::Failure(EKnightError::NotAuthority, TEXT("CompanionCommand"), FText::GetEmpty());
	}
	AKnightCharacter* Knight = GetControlledKnight();
	AKnightGameState* State = GetWorld() != nullptr ? GetWorld()->GetGameState<AKnightGameState>() : nullptr;
	AKnightPartyState* Party = State != nullptr ? State->GetPartyState() : nullptr;
	AKnightPlayerState* Human = GetPlayerState<AKnightPlayerState>();
	const int32 Slot = Party != nullptr ? Party->GetControlledSlot(this) : INDEX_NONE;
	const FKnightRosterEntry* Owned = Party != nullptr ? Party->FindSlot(Slot) : nullptr;
	if (Knight == nullptr || Owned == nullptr || Human == nullptr || !Human->GetHumanId().IsValid()
		|| Human->GetAssignedSlot() != Slot || Owned->HumanPlayerState != Human || Owned->Knight != Knight
		|| Owned->OwnerKind != EKnightControllerOwner::Human || Owned->HumanId != Human->GetHumanId()
		|| !ExpectedKnightId.IsValid() || !(ExpectedKnightId == Knight->GetKnightEntityId())
		|| !Knight->GetLifeComponent()->IsAlive())
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("CompanionCommand"),
			NSLOCTEXT("KnightArena", "CommandOwner", "A companion command requires your current living canonical Knight."));
	}
	AActor* Focus = nullptr;
	if (Command == EKnightCompanionCommand::Focus)
	{
		Focus = Knight->GetCombatComponent()->GetLockedTarget();
		UKnightAbilitySystemComponent* FocusASC = Focus != nullptr
			? Focus->FindComponentByClass<UKnightAbilitySystemComponent>() : nullptr;
		if (!FocusId.IsValid() || FocusASC == nullptr || !(FocusASC->GetEntityId() == FocusId))
		{
			return FKnightResult::Failure(EKnightError::InvalidId, TEXT("CompanionFocus"),
				NSLOCTEXT("KnightArena", "FocusLock", "Lock a living enemy with MMB before issuing Focus."));
		}
		const FKnightResult Target = Knight->GetCombatComponent()->ValidateCombatTarget(Focus, 2200.0f);
		if (!Target.IsSuccess())
		{
			return Target;
		}
	}
	else if (FocusId.IsValid())
	{
		return FKnightResult::Failure(EKnightError::InvalidData, TEXT("CompanionCommand"), FText::GetEmpty());
	}
	TArray<AKnightArenaCompanionController*> Candidates;
	for (const FKnightRosterEntry& Entry : Party->GetRoster())
	{
		AKnightArenaCompanionController* Companion = Cast<AKnightArenaCompanionController>(Entry.Knight->GetController());
		if (Companion == nullptr || Entry.OwnerKind != EKnightControllerOwner::Companion
			|| !Entry.Knight->GetLifeComponent()->IsAlive())
		{
			continue;
		}
		AKnightCharacter* NearestHuman = nullptr;
		double NearestDistance = TNumericLimits<double>::Max();
		for (const FKnightRosterEntry& HumanEntry : Party->GetRoster())
		{
			if (HumanEntry.OwnerKind == EKnightControllerOwner::Human && HumanEntry.Knight->GetLifeComponent()->IsAlive())
			{
				const double Distance = FVector::DistSquared(Entry.Knight->GetActorLocation(),
					HumanEntry.Knight->GetActorLocation());
				if (Distance < NearestDistance)
				{
					NearestHuman = HumanEntry.Knight;
					NearestDistance = Distance;
				}
			}
		}
		if (NearestHuman == Knight && Companion->GetArenaInitializationResult().IsSuccess()
			&& Companion->GetAIContext()->GetControlledPawn() == Entry.Knight)
		{
			Candidates.Add(Companion);
		}
	}
	if (Candidates.IsEmpty())
	{
		return FKnightResult::Failure(EKnightError::NotFound, TEXT("CompanionCommand"),
			NSLOCTEXT("KnightArena", "NoCompanions", "No living AI companion currently follows your Knight."));
	}
	for (AKnightArenaCompanionController* Companion : Candidates)
	{
		const FKnightResult Result = Companion->TrySetCommand(Command, Focus);
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	return FKnightResult::Success();
}

void AKnightArenaPlayerController::ServerRequestCompanionCommand_Implementation(
	const EKnightCompanionCommand Command, const FKnightEntityId ExpectedKnightId, const FKnightEntityId FocusId)
{
	const FKnightResult Result = ApplyCompanionCommand(Command, ExpectedKnightId, FocusId);
	ClientReceiveCompanionCommandResult(Command, Result);
}

void AKnightArenaPlayerController::ClientReceiveCompanionCommandResult_Implementation(
	const EKnightCompanionCommand Command, const FKnightResult Result)
{
	LastCompanionCommandResult = Result;
	if (Result.IsSuccess())
	{
		LastCompanionCommand = Command;
	}
	OnCompanionCommandResult.Broadcast(Result);
}

const FKnightResult& AKnightArenaPlayerController::GetLastCompanionCommandResult() const
{
	return LastCompanionCommandResult;
}

EKnightCompanionCommand AKnightArenaPlayerController::GetLastCompanionCommand() const
{
	return LastCompanionCommand;
}

void AKnightArenaPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
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
