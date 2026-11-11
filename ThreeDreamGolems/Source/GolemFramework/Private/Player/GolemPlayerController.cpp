#include "Player/GolemPlayerController.h"

#include "Character/GolemCharacter.h"
#include "Components/GolemAbilitySystemComponent.h"
#include "Components/GolemCombatComponent.h"
#include "Components/GolemLifeComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/GolemPartyGameMode.h"
#include "Input/GolemInputConfig.h"
#include "InputActionValue.h"
#include "Math/RotationMatrix.h"
#include "Party/GolemPartyState.h"
#include "Tags/GolemGameplayTags.h"
#include "Templates/UnrealTemplate.h"

DEFINE_LOG_CATEGORY_STATIC(LogGolemInput, Log, All);

AGolemPlayerController::AGolemPlayerController()
{
	bAutoManageActiveCameraTarget = true;
	bShowMouseCursor = false;
}

AGolemCharacter* AGolemPlayerController::GetControlledGolem() const
{
	return Cast<AGolemCharacter>(GetPawn());
}

UGolemInputConfig* AGolemPlayerController::GetGolemInputConfig() const
{
	return InputConfig;
}

int32 AGolemPlayerController::GetGameplayMappingPriority() const
{
	return GameplayMappingPriority;
}

const FGolemResult& AGolemPlayerController::GetLastCommandResult() const
{
	return LastCommandResult;
}

void AGolemPlayerController::RequestSwitchGolem(int32 DesiredSlot)
{
	ResetControlInput();
	ServerRequestSwitchGolem(DesiredSlot);
}

void AGolemPlayerController::RequestRestartDrill()
{
	ResetControlInput();
	ServerRequestRestartDrill();
}

void AGolemPlayerController::RequestAbilityInput(const FGameplayTag& InputTag, bool bPressed)
{
	const AGolemCharacter* Golem = GetControlledGolem();
	ServerRequestAbilityInput(InputTag, bPressed, Golem != nullptr ? Golem->GetGolemEntityId() : FGolemEntityId());
}

void AGolemPlayerController::RequestInteract()
{
	const AGolemCharacter* Golem = GetControlledGolem();
	ServerRequestInteract(Golem != nullptr ? Golem->GetGolemEntityId() : FGolemEntityId());
}

void AGolemPlayerController::ResetControlInput()
{
	FlushPressedKeys();
	if (AGolemCharacter* Golem = GetControlledGolem())
	{
		Golem->ResetControlInput();
	}
	if (HasAuthority() && !IsLocalController())
	{
		ClientResetControlInput();
	}
}

void AGolemPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
	UpdateControlledGolem();
}

void AGolemPlayerController::SetPlayer(UPlayer* InPlayer)
{
	RemoveLocalMapping();
	ResetControlInput();
	Super::SetPlayer(InPlayer);
	UpdateLocalMapping();
}

void AGolemPlayerController::BeginPlay()
{
	Super::BeginPlay();
	InitializeInputConfig();
	UpdateControlledGolem();
	UpdateLocalMapping();
	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void AGolemPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	RemoveCombatInputSource();
	RemoveAbilityResultSource();
	RemoveLocalMapping();
	RemoveOwnedInputBindings();
	FlushPressedKeys();
	if (AGolemCharacter* PreviousGolem = ControlledGolem.Get())
	{
		PreviousGolem->ResetControlInput();
		OnControlledGolemChanged.Broadcast(PreviousGolem, nullptr);
	}
	ControlledGolem.Reset();
	OnControlledGolemChanged.Clear();
	OnCommandResult.Clear();
	Super::EndPlay(EndPlayReason);
}

void AGolemPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InitializeInputConfig();
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInput == nullptr)
	{
		UE_LOG(LogGolemInput, Error, TEXT("Golem controller requires EnhancedInputComponent."));
		return;
	}
	if (BoundInputComponent.Get() == EnhancedInput && !InputBindingHandles.IsEmpty())
	{
		return;
	}
	RemoveOwnedInputBindings();
	BoundInputComponent = EnhancedInput;
	InputBindingHandles.Add(EnhancedInput->BindAction(InputConfig->FindAction(GolemGameplayTags::Input_Move),
		ETriggerEvent::Triggered, this, &AGolemPlayerController::HandleMove).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(InputConfig->FindAction(GolemGameplayTags::Input_Look),
		ETriggerEvent::Triggered, this, &AGolemPlayerController::HandleLook).GetHandle());
	const UInputAction* JumpAction = InputConfig->FindAction(GolemGameplayTags::Input_Jump);
	InputBindingHandles.Add(EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started,
		this, &AGolemPlayerController::HandleJumpStarted).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed,
		this, &AGolemPlayerController::HandleJumpEnded).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(JumpAction, ETriggerEvent::Canceled,
		this, &AGolemPlayerController::HandleJumpEnded).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(InputConfig->FindAction(GolemGameplayTags::Input_Golem_Next),
		ETriggerEvent::Started, this, &AGolemPlayerController::HandleSelectGolem, -1).GetHandle());
	const UInputAction* FirstAction = InputConfig->FindAction(GolemGameplayTags::Input_Golem_Select_First);
	const UInputAction* SecondAction = InputConfig->FindAction(GolemGameplayTags::Input_Golem_Select_Second);
	const UInputAction* ThirdAction = InputConfig->FindAction(GolemGameplayTags::Input_Golem_Select_Third);
	InputBindingHandles.Add(EnhancedInput->BindAction(FirstAction,
		ETriggerEvent::Started, this, &AGolemPlayerController::HandleSelectGolem, 0).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(SecondAction,
		ETriggerEvent::Started, this, &AGolemPlayerController::HandleSelectGolem, 1).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(ThirdAction,
		ETriggerEvent::Started, this, &AGolemPlayerController::HandleSelectGolem, 2).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(InputConfig->FindAction(GolemGameplayTags::Input_Training_Retry),
		ETriggerEvent::Started, this, &AGolemPlayerController::HandleRestartDrill).GetHandle());
	BindAbilityInput(GolemGameplayTags::Input_Ability_Test);
	BindAbilityInput(GolemGameplayTags::Input_Party_Revive);
	BindAbilityInput(GolemGameplayTags::Input_Combat_Primary);
	BindAbilityInput(GolemGameplayTags::Input_Combat_Heavy);
	BindAbilityInput(GolemGameplayTags::Input_Combat_Block);
	BindAbilityInput(GolemGameplayTags::Input_Combat_Parry);
	BindAbilityInput(GolemGameplayTags::Input_Combat_Dodge);
	BindAbilityInput(GolemGameplayTags::Input_Combat_LockOn);
	BindAbilityInput(GolemGameplayTags::Input_Combat_Rune);
	BindAbilityInput(GolemGameplayTags::Input_Combat_Finisher);
	BindAbilityInput(GolemGameplayTags::Input_World_Traverse);
	InputBindingHandles.Add(EnhancedInput->BindAction(InputConfig->FindAction(GolemGameplayTags::Input_World_Interact),
		ETriggerEvent::Started, this, &AGolemPlayerController::HandleInteract).GetHandle());
	UpdateLocalMapping();
}

void AGolemPlayerController::OnPossess(APawn* InPawn)
{
	FlushPressedKeys();
	Super::OnPossess(InPawn);
	UpdateControlledGolem();
}

void AGolemPlayerController::OnUnPossess()
{
	RemoveAbilityResultSource();
	ResetControlInput();
	Super::OnUnPossess();
	UpdateControlledGolem();
}

void AGolemPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	UpdateControlledGolem();
}

void AGolemPlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);
	FlushPressedKeys();
	UpdateControlledGolem();
	UpdateLocalMapping();
}

void AGolemPlayerController::PawnLeavingGame()
{
	AGolemCharacter* Golem = GetControlledGolem();
	if (Golem != nullptr && Golem->GetGolemEntityId().IsValid())
	{
		// GameMode owns takeover. UE's default implementation would destroy the canonical party actor.
		UnPossess();
		return;
	}
	Super::PawnLeavingGame();
}

void AGolemPlayerController::ServerRequestSwitchGolem_Implementation(int32 DesiredSlot)
{
	AGolemPartyGameMode* GameMode = GetWorld()->GetAuthGameMode<AGolemPartyGameMode>();
	const FGolemResult Result = GameMode != nullptr ? GameMode->TrySwitchGolem(this, DesiredSlot)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("SwitchGolem"),
			NSLOCTEXT("GolemInput", "NoPartyMode", "The current world does not have a party game mode."));
	ClientReceiveCommandResult(Result);
}

void AGolemPlayerController::ServerRequestRestartDrill_Implementation()
{
	AGolemPartyGameMode* GameMode = GetWorld()->GetAuthGameMode<AGolemPartyGameMode>();
	const FGolemResult Result = GameMode != nullptr ? GameMode->TryRestartDrill(this)
		: FGolemResult::Failure(EGolemError::InvalidState, TEXT("RestartDrill"),
			NSLOCTEXT("GolemInput", "NoTrainingMode", "The current world does not support a training restart."));
	ClientReceiveCommandResult(Result);
}

void AGolemPlayerController::ServerRequestAbilityInput_Implementation(
	FGameplayTag InputTag, bool bPressed, FGolemEntityId ExpectedGolemId)
{
	AGolemCharacter* Golem = nullptr;
	FGolemResult Result = ValidateGolemCommand(ExpectedGolemId, Golem);
	InitializeInputConfig();
	if (Result.IsSuccess() && InputTag != GolemGameplayTags::Input_Ability_Test
		&& InputTag != GolemGameplayTags::Input_Party_Revive && !InputConfig->IsCombatInput(InputTag))
	{
		Result = FGolemResult::Failure(EGolemError::InvalidData, TEXT("AbilityInput"),
			NSLOCTEXT("GolemInput", "AbilityInputUnbound", "The requested semantic ability input is not bound."));
	}
	if (Result.IsSuccess() && InputConfig->IsCombatInput(InputTag)
		&& Golem->GetCombatComponent()->GetProfile() == nullptr)
	{
		Result = FGolemResult::Failure(EGolemError::InvalidState, TEXT("AbilityInput"),
			NSLOCTEXT("GolemInput", "CombatNotConfigured", "Combat input requires the controlled Golem's combat profile."));
	}
	if (Result.IsSuccess())
	{
		const TGuardValue<bool> ProcessingScope(bProcessingAbilityInput, true);
		AActor* Target = nullptr;
		if (InputConfig->IsCombatInput(InputTag))
		{
			UGolemCombatComponent* Combat = Golem->GetCombatComponent();
			if (InputTag == GolemGameplayTags::Input_Combat_Finisher)
			{
				Target = Combat->FindCombatTarget(220.0f, true);
			}
			else
			{
				const float Range = InputTag == GolemGameplayTags::Input_Combat_Rune
					? Combat->GetProfile()->GetTuning().RuneRange : Combat->GetProfile()->GetTuning().LockRange;
				Target = Combat->GetLockedTarget();
				if (Target == nullptr || !Combat->ValidateCombatTarget(Target, Range).IsSuccess())
				{
					Target = Combat->FindCombatTarget(Range);
				}
			}
		}
		Result = Golem->GetGolemAbilitySystem()->RequestAbilityInput(InputTag, bPressed, Target);
	}
	if (bPressed || !Result.IsSuccess())
	{
		ClientReceiveCommandResult(Result);
	}
}

void AGolemPlayerController::ServerRequestInteract_Implementation(FGolemEntityId ExpectedGolemId)
{
	AGolemCharacter* Golem = nullptr;
	FGolemResult Result = ValidateGolemCommand(ExpectedGolemId, Golem);
	if (Result.IsSuccess() && !Golem->GetLifeComponent()->IsAlive())
	{
		Result = FGolemResult::Failure(EGolemError::InvalidState, TEXT("Interact"),
			NSLOCTEXT("GolemInput", "InteractLifeBlocked", "A downed or dead Golem cannot use a test station."));
	}
	if (Result.IsSuccess())
	{
		AGolemPartyGameMode* Mode = GetWorld()->GetAuthGameMode<AGolemPartyGameMode>();
		Result = Mode->TryInteract(this);
	}
	ClientReceiveCommandResult(Result);
}

void AGolemPlayerController::ClientReceiveCommandResult_Implementation(const FGolemResult& Result)
{
	LastCommandResult = Result;
	OnCommandResult.Broadcast(LastCommandResult);
}

void AGolemPlayerController::ClientResetControlInput_Implementation()
{
	FlushPressedKeys();
	if (AGolemCharacter* Golem = GetControlledGolem())
	{
		Golem->ResetControlInput();
	}
}

void AGolemPlayerController::InitializeInputConfig()
{
	if (InputConfig == nullptr)
	{
		InputConfig = NewObject<UGolemInputConfig>(this);
		InputConfig->InitializeDefaultMapping();
	}
}

void AGolemPlayerController::UpdateControlledGolem()
{
	AGolemCharacter* CurrentGolem = GetControlledGolem();
	AGolemCharacter* PreviousGolem = ControlledGolem.Get();
	if (CurrentGolem == PreviousGolem)
	{
		UpdateAbilityResultSource();
		UpdateCombatInputSource();
		return;
	}
	RemoveAbilityResultSource();
	RemoveCombatInputSource();
	FlushPressedKeys();
	if (PreviousGolem != nullptr)
	{
		PreviousGolem->ResetControlInput();
	}
	if (CurrentGolem != nullptr)
	{
		CurrentGolem->ResetControlInput();
	}
	ControlledGolem = CurrentGolem;
	UpdateAbilityResultSource();
	UpdateCombatInputSource();
	UpdateLocalMapping();
	OnControlledGolemChanged.Broadcast(PreviousGolem, CurrentGolem);
}

void AGolemPlayerController::UpdateLocalMapping()
{
	if (!IsLocalController() || GetControlledGolem() == nullptr)
	{
		RemoveLocalMapping();
		return;
	}
	InitializeInputConfig();
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* LocalSubsystem = LocalPlayer != nullptr
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (LocalSubsystem == MappingSubsystem.Get())
	{
		UpdateCombatMapping();
		return;
	}
	RemoveLocalMapping();
	if (LocalSubsystem != nullptr)
	{
		FModifyContextOptions Options;
		Options.bIgnoreAllPressedKeysUntilRelease = true;
		LocalSubsystem->AddMappingContext(InputConfig->GetMappingContext(), GameplayMappingPriority, Options);
		MappingSubsystem = LocalSubsystem;
		UpdateCombatMapping();
	}
}

void AGolemPlayerController::RemoveLocalMapping()
{
	if (UEnhancedInputLocalPlayerSubsystem* LocalSubsystem = MappingSubsystem.Get())
	{
		if (InputConfig != nullptr)
		{
			LocalSubsystem->RemoveMappingContext(InputConfig->GetMappingContext());
			if (bCombatMappingAdded)
			{
				LocalSubsystem->RemoveMappingContext(InputConfig->GetCombatMappingContext());
			}
		}
	}
	MappingSubsystem.Reset();
	bCombatMappingAdded = false;
}

void AGolemPlayerController::UpdateCombatInputSource()
{
	AGolemCharacter* Golem = GetControlledGolem();
	UGolemCombatComponent* Combat = IsValid(Golem) ? Golem->GetCombatComponent() : nullptr;
	if (bEndingPlay || CombatInputSource.Get() == Combat)
	{
		UpdateCombatMapping();
		return;
	}
	RemoveCombatInputSource();
	if (Combat != nullptr)
	{
		CombatInputSource = Combat;
		CombatProfileHandle = Combat->OnProfileChanged.AddUObject(this, &ThisClass::HandleCombatProfileChanged);
	}
	UpdateCombatMapping();
}

void AGolemPlayerController::RemoveCombatInputSource()
{
	if (UGolemCombatComponent* Combat = CombatInputSource.Get())
	{
		Combat->OnProfileChanged.Remove(CombatProfileHandle);
	}
	CombatProfileHandle.Reset();
	CombatInputSource.Reset();
	UpdateCombatMapping();
}

void AGolemPlayerController::UpdateCombatMapping()
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = MappingSubsystem.Get();
	if (Subsystem == nullptr || InputConfig == nullptr)
	{
		return;
	}
	const AGolemCharacter* Golem = GetControlledGolem();
	const bool bNeedsCombat = !bEndingPlay && IsLocalController() && Golem != nullptr
		&& Golem->GetCombatComponent() == CombatInputSource.Get() && CombatInputSource.IsValid()
		&& CombatInputSource->GetProfile() != nullptr;
	if (bNeedsCombat == bCombatMappingAdded)
	{
		return;
	}
	FlushPressedKeys();
	if (bNeedsCombat)
	{
		FModifyContextOptions Options;
		Options.bIgnoreAllPressedKeysUntilRelease = true;
		Subsystem->AddMappingContext(InputConfig->GetCombatMappingContext(), GameplayMappingPriority + 100, Options);
	}
	else
	{
		Subsystem->RemoveMappingContext(InputConfig->GetCombatMappingContext());
	}
	bCombatMappingAdded = bNeedsCombat;
}

void AGolemPlayerController::HandleCombatProfileChanged(UGolemCombatProfileDefinition* Profile)
{
	UpdateCombatMapping();
}

void AGolemPlayerController::RemoveOwnedInputBindings()
{
	if (UEnhancedInputComponent* EnhancedInput = BoundInputComponent.Get())
	{
		for (const uint32 BindingHandle : InputBindingHandles)
		{
			EnhancedInput->RemoveBindingByHandle(BindingHandle);
		}
	}
	InputBindingHandles.Reset();
	BoundInputComponent.Reset();
}

void AGolemPlayerController::BindAbilityInput(const FGameplayTag& InputTag)
{
	UEnhancedInputComponent* EnhancedInput = BoundInputComponent.Get();
	const UInputAction* Action = InputConfig->FindAction(InputTag);
	InputBindingHandles.Add(EnhancedInput->BindAction(Action, ETriggerEvent::Started,
		this, &AGolemPlayerController::HandleAbilityPressed, FGameplayTag(InputTag)).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(Action, ETriggerEvent::Completed,
		this, &AGolemPlayerController::HandleAbilityReleased, FGameplayTag(InputTag)).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(Action, ETriggerEvent::Canceled,
		this, &AGolemPlayerController::HandleAbilityReleased, FGameplayTag(InputTag)).GetHandle());
}

void AGolemPlayerController::UpdateAbilityResultSource()
{
	AGolemCharacter* Golem = GetControlledGolem();
	UGolemAbilitySystemComponent* ASC = IsValid(Golem) ? Golem->GetGolemAbilitySystem() : nullptr;
	const UWorld* World = GetWorld();
	if (!HasAuthority() || bEndingPlay || World == nullptr || World->bIsTearingDown
		|| ASC == nullptr || !Golem->GetGolemEntityId().IsValid())
	{
		RemoveAbilityResultSource();
		return;
	}
	if (AbilityResultSource.Get() == ASC && AbilityResultGolemId == Golem->GetGolemEntityId()
		&& AbilityResultHandle.IsValid())
	{
		return;
	}
	RemoveAbilityResultSource();
	AbilityResultSource = ASC;
	AbilityResultGolemId = Golem->GetGolemEntityId();
	AbilityResultHandle = ASC->OnAbilityResult.AddUObject(this, &ThisClass::HandleAbilityResult);
}

void AGolemPlayerController::RemoveAbilityResultSource()
{
	if (UGolemAbilitySystemComponent* ASC = AbilityResultSource.Get())
	{
		ASC->OnAbilityResult.Remove(AbilityResultHandle);
	}
	AbilityResultSource.Reset();
	AbilityResultGolemId = FGolemEntityId();
	AbilityResultHandle.Reset();
}

void AGolemPlayerController::HandleAbilityResult(const FGolemResult& Result)
{
	if (bProcessingAbilityInput || bEndingPlay || !AbilityResultSource.IsValid())
	{
		return;
	}
	AGolemCharacter* Golem = nullptr;
	if (ValidateGolemCommand(AbilityResultGolemId, Golem).IsSuccess()
		&& Golem->GetGolemAbilitySystem() == AbilityResultSource.Get())
	{
		ClientReceiveCommandResult(Result);
	}
}

FGolemResult AGolemPlayerController::ValidateGolemCommand(
	const FGolemEntityId& ExpectedGolemId, AGolemCharacter*& OutGolem) const
{
	OutGolem = nullptr;
	const UWorld* World = GetWorld();
	if (!HasAuthority() || World == nullptr || World->bIsTearingDown || World->IsInSeamlessTravel())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("GolemCommand"),
			NSLOCTEXT("GolemInput", "CommandWorldUnavailable", "The authoritative Golem world is unavailable."));
	}
	const AGolemPartyGameMode* Mode = World->GetAuthGameMode<AGolemPartyGameMode>();
	const AGolemPartyState* Party = Mode != nullptr ? Mode->GetPartyState() : nullptr;
	AGolemCharacter* Golem = GetControlledGolem();
	const FGolemRosterEntry* Entry = Party != nullptr ? Party->FindGolem(Golem) : nullptr;
	if (!IsValid(Golem) || Golem->GetWorld() != World || Golem->GetController() != this
		|| Entry == nullptr || !Party->IsCanonicalRosterReady() || Entry->OwnerKind != EGolemControllerOwner::Human
		|| Entry->Controller != this || Entry->GolemId != Golem->GetGolemEntityId()
		|| !ExpectedGolemId.IsValid() || ExpectedGolemId != Golem->GetGolemEntityId())
	{
		return FGolemResult::Failure(EGolemError::NotOwned, TEXT("GolemCommand"),
			NSLOCTEXT("GolemInput", "CommandWrongGolem", "The command does not match the human's current canonical Golem."));
	}
	UGolemAbilitySystemComponent* ASC = Golem->GetGolemAbilitySystem();
	if (ASC == nullptr || ASC->GetOwnerActor() != Golem || ASC->GetAvatarActor() != Golem
		|| !Golem->GetAbilityInitializationResult().IsSuccess())
	{
		return FGolemResult::Failure(EGolemError::InvalidState, TEXT("GolemCommand"),
			NSLOCTEXT("GolemInput", "CommandAbilitiesUnavailable", "The controlled Golem's ability system is not ready."));
	}
	OutGolem = Golem;
	return FGolemResult::Success();
}

void AGolemPlayerController::HandleMove(const FInputActionValue& Value)
{
	AGolemCharacter* Golem = GetControlledGolem();
	if (Golem == nullptr || Golem->GetController() != this || IsMoveInputIgnored()
		|| !Golem->GetLifeComponent()->IsAlive()
		|| Golem->GetGolemAbilitySystem()->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Staggered))
	{
		return;
	}
	const FVector2D Movement = Value.Get<FVector2D>().GetClampedToMaxSize(1.0);
	const FRotator YawRotation(0.0f, GetControlRotation().Yaw, 0.0f);
	Golem->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Movement.Y);
	Golem->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Movement.X);
}

void AGolemPlayerController::HandleLook(const FInputActionValue& Value)
{
	if (GetControlledGolem() == nullptr || IsLookInputIgnored())
	{
		return;
	}
	const FVector2D Look = Value.Get<FVector2D>();
	AddYawInput(Look.X * LookSensitivity);
	AddPitchInput(Look.Y * LookSensitivity * (bInvertLookY ? -1.0f : 1.0f));
}

void AGolemPlayerController::SetLocalInputPreferences(const float Sensitivity, const bool bInvertY,
	const bool bToggleBlock, const bool bHoldLock)
{
	if (!FMath::IsFinite(Sensitivity) || Sensitivity < 0.1f || Sensitivity > 4.0f)
	{
		return;
	}
	ResetControlInput();
	LookSensitivity = Sensitivity;
	bInvertLookY = bInvertY;
	bToggleBlockInput = bToggleBlock;
	bHoldLockInput = bHoldLock;
}

void AGolemPlayerController::HandleJumpStarted()
{
	if (AGolemCharacter* Golem = GetControlledGolem(); Golem != nullptr && Golem->GetController() == this
		&& Golem->GetLifeComponent()->IsAlive() && !IsMoveInputIgnored()
		&& !Golem->GetGolemAbilitySystem()->HasMatchingGameplayTag(GolemGameplayTags::State_Combat_Staggered))
	{
		Golem->Jump();
	}
}

void AGolemPlayerController::HandleJumpEnded()
{
	if (AGolemCharacter* Golem = GetControlledGolem())
	{
		Golem->StopJumping();
	}
}

void AGolemPlayerController::HandleSelectGolem(int32 DesiredSlot)
{
	RequestSwitchGolem(DesiredSlot);
}

void AGolemPlayerController::HandleRestartDrill()
{
	RequestRestartDrill();
}

void AGolemPlayerController::HandleAbilityPressed(FGameplayTag InputTag)
{
	if (bToggleBlockInput && InputTag == GolemGameplayTags::Input_Combat_Block && GetControlledGolem() != nullptr)
	{
		RequestAbilityInput(InputTag, !GetControlledGolem()->GetCombatComponent()->GetSnapshot().bBlocking);
		return;
	}
	RequestAbilityInput(InputTag, true);
}

void AGolemPlayerController::HandleAbilityReleased(FGameplayTag InputTag)
{
	if (bToggleBlockInput && InputTag == GolemGameplayTags::Input_Combat_Block)
	{
		return;
	}
	if (bHoldLockInput && InputTag == GolemGameplayTags::Input_Combat_LockOn
		&& GetControlledGolem() != nullptr && GetControlledGolem()->GetCombatComponent()->GetLockedTarget() != nullptr)
	{
		RequestAbilityInput(InputTag, true);
		return;
	}
	RequestAbilityInput(InputTag, false);
}

void AGolemPlayerController::HandleInteract()
{
	RequestInteract();
}
