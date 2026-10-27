#include "Player/KnightPlayerController.h"

#include "Character/KnightCharacter.h"
#include "Components/KnightAbilitySystemComponent.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/KnightPartyGameMode.h"
#include "Input/KnightInputConfig.h"
#include "InputActionValue.h"
#include "Math/RotationMatrix.h"
#include "Party/KnightPartyState.h"
#include "Tags/KnightGameplayTags.h"
#include "Templates/UnrealTemplate.h"

DEFINE_LOG_CATEGORY_STATIC(LogKnightInput, Log, All);

AKnightPlayerController::AKnightPlayerController()
{
	bAutoManageActiveCameraTarget = true;
	bShowMouseCursor = false;
}

AKnightCharacter* AKnightPlayerController::GetControlledKnight() const
{
	return Cast<AKnightCharacter>(GetPawn());
}

UKnightInputConfig* AKnightPlayerController::GetKnightInputConfig() const
{
	return InputConfig;
}

int32 AKnightPlayerController::GetGameplayMappingPriority() const
{
	return GameplayMappingPriority;
}

const FKnightResult& AKnightPlayerController::GetLastCommandResult() const
{
	return LastCommandResult;
}

void AKnightPlayerController::RequestSwitchKnight(int32 DesiredSlot)
{
	ResetControlInput();
	ServerRequestSwitchKnight(DesiredSlot);
}

void AKnightPlayerController::RequestRestartDrill()
{
	ResetControlInput();
	ServerRequestRestartDrill();
}

void AKnightPlayerController::RequestAbilityInput(const FGameplayTag& InputTag, bool bPressed)
{
	const AKnightCharacter* Knight = GetControlledKnight();
	ServerRequestAbilityInput(InputTag, bPressed, Knight != nullptr ? Knight->GetKnightEntityId() : FKnightEntityId());
}

void AKnightPlayerController::RequestInteract()
{
	const AKnightCharacter* Knight = GetControlledKnight();
	ServerRequestInteract(Knight != nullptr ? Knight->GetKnightEntityId() : FKnightEntityId());
}

void AKnightPlayerController::ResetControlInput()
{
	FlushPressedKeys();
	if (AKnightCharacter* Knight = GetControlledKnight())
	{
		Knight->ResetControlInput();
	}
	if (HasAuthority() && !IsLocalController())
	{
		ClientResetControlInput();
	}
}

void AKnightPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
	UpdateControlledKnight();
}

void AKnightPlayerController::SetPlayer(UPlayer* InPlayer)
{
	RemoveLocalMapping();
	ResetControlInput();
	Super::SetPlayer(InPlayer);
	UpdateLocalMapping();
}

void AKnightPlayerController::BeginPlay()
{
	Super::BeginPlay();
	InitializeInputConfig();
	UpdateControlledKnight();
	UpdateLocalMapping();
	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void AKnightPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	RemoveCombatInputSource();
	RemoveAbilityResultSource();
	RemoveLocalMapping();
	RemoveOwnedInputBindings();
	FlushPressedKeys();
	if (AKnightCharacter* PreviousKnight = ControlledKnight.Get())
	{
		PreviousKnight->ResetControlInput();
		OnControlledKnightChanged.Broadcast(PreviousKnight, nullptr);
	}
	ControlledKnight.Reset();
	OnControlledKnightChanged.Clear();
	OnCommandResult.Clear();
	Super::EndPlay(EndPlayReason);
}

void AKnightPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InitializeInputConfig();
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInput == nullptr)
	{
		UE_LOG(LogKnightInput, Error, TEXT("Knight controller requires EnhancedInputComponent."));
		return;
	}
	if (BoundInputComponent.Get() == EnhancedInput && !InputBindingHandles.IsEmpty())
	{
		return;
	}
	RemoveOwnedInputBindings();
	BoundInputComponent = EnhancedInput;
	InputBindingHandles.Add(EnhancedInput->BindAction(InputConfig->FindAction(KnightGameplayTags::Input_Move),
		ETriggerEvent::Triggered, this, &AKnightPlayerController::HandleMove).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(InputConfig->FindAction(KnightGameplayTags::Input_Look),
		ETriggerEvent::Triggered, this, &AKnightPlayerController::HandleLook).GetHandle());
	const UInputAction* JumpAction = InputConfig->FindAction(KnightGameplayTags::Input_Jump);
	InputBindingHandles.Add(EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started,
		this, &AKnightPlayerController::HandleJumpStarted).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed,
		this, &AKnightPlayerController::HandleJumpEnded).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(JumpAction, ETriggerEvent::Canceled,
		this, &AKnightPlayerController::HandleJumpEnded).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(InputConfig->FindAction(KnightGameplayTags::Input_Knight_Next),
		ETriggerEvent::Started, this, &AKnightPlayerController::HandleSelectKnight, -1).GetHandle());
	const UInputAction* FirstAction = InputConfig->FindAction(KnightGameplayTags::Input_Knight_Select_First);
	const UInputAction* SecondAction = InputConfig->FindAction(KnightGameplayTags::Input_Knight_Select_Second);
	const UInputAction* ThirdAction = InputConfig->FindAction(KnightGameplayTags::Input_Knight_Select_Third);
	InputBindingHandles.Add(EnhancedInput->BindAction(FirstAction,
		ETriggerEvent::Started, this, &AKnightPlayerController::HandleSelectKnight, 0).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(SecondAction,
		ETriggerEvent::Started, this, &AKnightPlayerController::HandleSelectKnight, 1).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(ThirdAction,
		ETriggerEvent::Started, this, &AKnightPlayerController::HandleSelectKnight, 2).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(InputConfig->FindAction(KnightGameplayTags::Input_Training_Retry),
		ETriggerEvent::Started, this, &AKnightPlayerController::HandleRestartDrill).GetHandle());
	BindAbilityInput(KnightGameplayTags::Input_Ability_Test);
	BindAbilityInput(KnightGameplayTags::Input_Party_Revive);
	BindAbilityInput(KnightGameplayTags::Input_Combat_Primary);
	BindAbilityInput(KnightGameplayTags::Input_Combat_Heavy);
	BindAbilityInput(KnightGameplayTags::Input_Combat_Block);
	BindAbilityInput(KnightGameplayTags::Input_Combat_Parry);
	BindAbilityInput(KnightGameplayTags::Input_Combat_Dodge);
	BindAbilityInput(KnightGameplayTags::Input_Combat_LockOn);
	BindAbilityInput(KnightGameplayTags::Input_Combat_Rune);
	BindAbilityInput(KnightGameplayTags::Input_Combat_Finisher);
	BindAbilityInput(KnightGameplayTags::Input_World_Traverse);
	InputBindingHandles.Add(EnhancedInput->BindAction(InputConfig->FindAction(KnightGameplayTags::Input_World_Interact),
		ETriggerEvent::Started, this, &AKnightPlayerController::HandleInteract).GetHandle());
	UpdateLocalMapping();
}

void AKnightPlayerController::OnPossess(APawn* InPawn)
{
	FlushPressedKeys();
	Super::OnPossess(InPawn);
	UpdateControlledKnight();
}

void AKnightPlayerController::OnUnPossess()
{
	RemoveAbilityResultSource();
	ResetControlInput();
	Super::OnUnPossess();
	UpdateControlledKnight();
}

void AKnightPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();
	UpdateControlledKnight();
}

void AKnightPlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);
	FlushPressedKeys();
	UpdateControlledKnight();
	UpdateLocalMapping();
}

void AKnightPlayerController::PawnLeavingGame()
{
	AKnightCharacter* Knight = GetControlledKnight();
	if (Knight != nullptr && Knight->GetKnightEntityId().IsValid())
	{
		// GameMode owns takeover. UE's default implementation would destroy the canonical party actor.
		UnPossess();
		return;
	}
	Super::PawnLeavingGame();
}

void AKnightPlayerController::ServerRequestSwitchKnight_Implementation(int32 DesiredSlot)
{
	AKnightPartyGameMode* GameMode = GetWorld()->GetAuthGameMode<AKnightPartyGameMode>();
	const FKnightResult Result = GameMode != nullptr ? GameMode->TrySwitchKnight(this, DesiredSlot)
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("SwitchKnight"),
			NSLOCTEXT("KnightInput", "NoPartyMode", "The current world does not have a party game mode."));
	ClientReceiveCommandResult(Result);
}

void AKnightPlayerController::ServerRequestRestartDrill_Implementation()
{
	AKnightPartyGameMode* GameMode = GetWorld()->GetAuthGameMode<AKnightPartyGameMode>();
	const FKnightResult Result = GameMode != nullptr ? GameMode->TryRestartDrill(this)
		: FKnightResult::Failure(EKnightError::InvalidState, TEXT("RestartDrill"),
			NSLOCTEXT("KnightInput", "NoTrainingMode", "The current world does not support a training restart."));
	ClientReceiveCommandResult(Result);
}

void AKnightPlayerController::ServerRequestAbilityInput_Implementation(
	FGameplayTag InputTag, bool bPressed, FKnightEntityId ExpectedKnightId)
{
	AKnightCharacter* Knight = nullptr;
	FKnightResult Result = ValidateKnightCommand(ExpectedKnightId, Knight);
	InitializeInputConfig();
	if (Result.IsSuccess() && InputTag != KnightGameplayTags::Input_Ability_Test
		&& InputTag != KnightGameplayTags::Input_Party_Revive && !InputConfig->IsCombatInput(InputTag))
	{
		Result = FKnightResult::Failure(EKnightError::InvalidData, TEXT("AbilityInput"),
			NSLOCTEXT("KnightInput", "AbilityInputUnbound", "The requested semantic ability input is not bound."));
	}
	if (Result.IsSuccess() && InputConfig->IsCombatInput(InputTag)
		&& Knight->GetCombatComponent()->GetProfile() == nullptr)
	{
		Result = FKnightResult::Failure(EKnightError::InvalidState, TEXT("AbilityInput"),
			NSLOCTEXT("KnightInput", "CombatNotConfigured", "Combat input requires the controlled Knight's combat profile."));
	}
	if (Result.IsSuccess())
	{
		const TGuardValue<bool> ProcessingScope(bProcessingAbilityInput, true);
		AActor* Target = nullptr;
		if (InputConfig->IsCombatInput(InputTag))
		{
			UKnightCombatComponent* Combat = Knight->GetCombatComponent();
			if (InputTag == KnightGameplayTags::Input_Combat_Finisher)
			{
				Target = Combat->FindCombatTarget(220.0f, true);
			}
			else
			{
				const float Range = InputTag == KnightGameplayTags::Input_Combat_Rune
					? Combat->GetProfile()->GetTuning().RuneRange : Combat->GetProfile()->GetTuning().LockRange;
				Target = Combat->GetLockedTarget();
				if (Target == nullptr || !Combat->ValidateCombatTarget(Target, Range).IsSuccess())
				{
					Target = Combat->FindCombatTarget(Range);
				}
			}
		}
		Result = Knight->GetKnightAbilitySystem()->RequestAbilityInput(InputTag, bPressed, Target);
	}
	if (bPressed || !Result.IsSuccess())
	{
		ClientReceiveCommandResult(Result);
	}
}

void AKnightPlayerController::ServerRequestInteract_Implementation(FKnightEntityId ExpectedKnightId)
{
	AKnightCharacter* Knight = nullptr;
	FKnightResult Result = ValidateKnightCommand(ExpectedKnightId, Knight);
	if (Result.IsSuccess() && !Knight->GetLifeComponent()->IsAlive())
	{
		Result = FKnightResult::Failure(EKnightError::InvalidState, TEXT("Interact"),
			NSLOCTEXT("KnightInput", "InteractLifeBlocked", "A downed or dead Knight cannot use a test station."));
	}
	if (Result.IsSuccess())
	{
		AKnightPartyGameMode* Mode = GetWorld()->GetAuthGameMode<AKnightPartyGameMode>();
		Result = Mode->TryInteract(this);
	}
	ClientReceiveCommandResult(Result);
}

void AKnightPlayerController::ClientReceiveCommandResult_Implementation(const FKnightResult& Result)
{
	LastCommandResult = Result;
	OnCommandResult.Broadcast(LastCommandResult);
}

void AKnightPlayerController::ClientResetControlInput_Implementation()
{
	FlushPressedKeys();
	if (AKnightCharacter* Knight = GetControlledKnight())
	{
		Knight->ResetControlInput();
	}
}

void AKnightPlayerController::InitializeInputConfig()
{
	if (InputConfig == nullptr)
	{
		InputConfig = NewObject<UKnightInputConfig>(this);
		InputConfig->InitializeDefaultMapping();
	}
}

void AKnightPlayerController::UpdateControlledKnight()
{
	AKnightCharacter* CurrentKnight = GetControlledKnight();
	AKnightCharacter* PreviousKnight = ControlledKnight.Get();
	if (CurrentKnight == PreviousKnight)
	{
		UpdateAbilityResultSource();
		UpdateCombatInputSource();
		return;
	}
	RemoveAbilityResultSource();
	RemoveCombatInputSource();
	FlushPressedKeys();
	if (PreviousKnight != nullptr)
	{
		PreviousKnight->ResetControlInput();
	}
	if (CurrentKnight != nullptr)
	{
		CurrentKnight->ResetControlInput();
	}
	ControlledKnight = CurrentKnight;
	UpdateAbilityResultSource();
	UpdateCombatInputSource();
	UpdateLocalMapping();
	OnControlledKnightChanged.Broadcast(PreviousKnight, CurrentKnight);
}

void AKnightPlayerController::UpdateLocalMapping()
{
	if (!IsLocalController() || GetControlledKnight() == nullptr)
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

void AKnightPlayerController::RemoveLocalMapping()
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

void AKnightPlayerController::UpdateCombatInputSource()
{
	AKnightCharacter* Knight = GetControlledKnight();
	UKnightCombatComponent* Combat = IsValid(Knight) ? Knight->GetCombatComponent() : nullptr;
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

void AKnightPlayerController::RemoveCombatInputSource()
{
	if (UKnightCombatComponent* Combat = CombatInputSource.Get())
	{
		Combat->OnProfileChanged.Remove(CombatProfileHandle);
	}
	CombatProfileHandle.Reset();
	CombatInputSource.Reset();
	UpdateCombatMapping();
}

void AKnightPlayerController::UpdateCombatMapping()
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = MappingSubsystem.Get();
	if (Subsystem == nullptr || InputConfig == nullptr)
	{
		return;
	}
	const AKnightCharacter* Knight = GetControlledKnight();
	const bool bNeedsCombat = !bEndingPlay && IsLocalController() && Knight != nullptr
		&& Knight->GetCombatComponent() == CombatInputSource.Get() && CombatInputSource.IsValid()
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

void AKnightPlayerController::HandleCombatProfileChanged(UKnightCombatProfileDefinition* Profile)
{
	UpdateCombatMapping();
}

void AKnightPlayerController::RemoveOwnedInputBindings()
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

void AKnightPlayerController::BindAbilityInput(const FGameplayTag& InputTag)
{
	UEnhancedInputComponent* EnhancedInput = BoundInputComponent.Get();
	const UInputAction* Action = InputConfig->FindAction(InputTag);
	InputBindingHandles.Add(EnhancedInput->BindAction(Action, ETriggerEvent::Started,
		this, &AKnightPlayerController::HandleAbilityPressed, FGameplayTag(InputTag)).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(Action, ETriggerEvent::Completed,
		this, &AKnightPlayerController::HandleAbilityReleased, FGameplayTag(InputTag)).GetHandle());
	InputBindingHandles.Add(EnhancedInput->BindAction(Action, ETriggerEvent::Canceled,
		this, &AKnightPlayerController::HandleAbilityReleased, FGameplayTag(InputTag)).GetHandle());
}

void AKnightPlayerController::UpdateAbilityResultSource()
{
	AKnightCharacter* Knight = GetControlledKnight();
	UKnightAbilitySystemComponent* ASC = IsValid(Knight) ? Knight->GetKnightAbilitySystem() : nullptr;
	const UWorld* World = GetWorld();
	if (!HasAuthority() || bEndingPlay || World == nullptr || World->bIsTearingDown
		|| ASC == nullptr || !Knight->GetKnightEntityId().IsValid())
	{
		RemoveAbilityResultSource();
		return;
	}
	if (AbilityResultSource.Get() == ASC && AbilityResultKnightId == Knight->GetKnightEntityId()
		&& AbilityResultHandle.IsValid())
	{
		return;
	}
	RemoveAbilityResultSource();
	AbilityResultSource = ASC;
	AbilityResultKnightId = Knight->GetKnightEntityId();
	AbilityResultHandle = ASC->OnAbilityResult.AddUObject(this, &ThisClass::HandleAbilityResult);
}

void AKnightPlayerController::RemoveAbilityResultSource()
{
	if (UKnightAbilitySystemComponent* ASC = AbilityResultSource.Get())
	{
		ASC->OnAbilityResult.Remove(AbilityResultHandle);
	}
	AbilityResultSource.Reset();
	AbilityResultKnightId = FKnightEntityId();
	AbilityResultHandle.Reset();
}

void AKnightPlayerController::HandleAbilityResult(const FKnightResult& Result)
{
	if (bProcessingAbilityInput || bEndingPlay || !AbilityResultSource.IsValid())
	{
		return;
	}
	AKnightCharacter* Knight = nullptr;
	if (ValidateKnightCommand(AbilityResultKnightId, Knight).IsSuccess()
		&& Knight->GetKnightAbilitySystem() == AbilityResultSource.Get())
	{
		ClientReceiveCommandResult(Result);
	}
}

FKnightResult AKnightPlayerController::ValidateKnightCommand(
	const FKnightEntityId& ExpectedKnightId, AKnightCharacter*& OutKnight) const
{
	OutKnight = nullptr;
	const UWorld* World = GetWorld();
	if (!HasAuthority() || World == nullptr || World->bIsTearingDown || World->IsInSeamlessTravel())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("KnightCommand"),
			NSLOCTEXT("KnightInput", "CommandWorldUnavailable", "The authoritative Knight world is unavailable."));
	}
	const AKnightPartyGameMode* Mode = World->GetAuthGameMode<AKnightPartyGameMode>();
	const AKnightPartyState* Party = Mode != nullptr ? Mode->GetPartyState() : nullptr;
	AKnightCharacter* Knight = GetControlledKnight();
	const FKnightRosterEntry* Entry = Party != nullptr ? Party->FindKnight(Knight) : nullptr;
	if (!IsValid(Knight) || Knight->GetWorld() != World || Knight->GetController() != this
		|| Entry == nullptr || !Party->IsCanonicalRosterReady() || Entry->OwnerKind != EKnightControllerOwner::Human
		|| Entry->Controller != this || Entry->KnightId != Knight->GetKnightEntityId()
		|| !ExpectedKnightId.IsValid() || ExpectedKnightId != Knight->GetKnightEntityId())
	{
		return FKnightResult::Failure(EKnightError::NotOwned, TEXT("KnightCommand"),
			NSLOCTEXT("KnightInput", "CommandWrongKnight", "The command does not match the human's current canonical Knight."));
	}
	UKnightAbilitySystemComponent* ASC = Knight->GetKnightAbilitySystem();
	if (ASC == nullptr || ASC->GetOwnerActor() != Knight || ASC->GetAvatarActor() != Knight
		|| !Knight->GetAbilityInitializationResult().IsSuccess())
	{
		return FKnightResult::Failure(EKnightError::InvalidState, TEXT("KnightCommand"),
			NSLOCTEXT("KnightInput", "CommandAbilitiesUnavailable", "The controlled Knight's ability system is not ready."));
	}
	OutKnight = Knight;
	return FKnightResult::Success();
}

void AKnightPlayerController::HandleMove(const FInputActionValue& Value)
{
	AKnightCharacter* Knight = GetControlledKnight();
	if (Knight == nullptr || Knight->GetController() != this || IsMoveInputIgnored()
		|| !Knight->GetLifeComponent()->IsAlive()
		|| Knight->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Staggered))
	{
		return;
	}
	const FVector2D Movement = Value.Get<FVector2D>().GetClampedToMaxSize(1.0);
	const FRotator YawRotation(0.0f, GetControlRotation().Yaw, 0.0f);
	Knight->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Movement.Y);
	Knight->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Movement.X);
}

void AKnightPlayerController::HandleLook(const FInputActionValue& Value)
{
	if (GetControlledKnight() == nullptr || IsLookInputIgnored())
	{
		return;
	}
	const FVector2D Look = Value.Get<FVector2D>();
	AddYawInput(Look.X * LookSensitivity);
	AddPitchInput(Look.Y * LookSensitivity * (bInvertLookY ? -1.0f : 1.0f));
}

void AKnightPlayerController::SetLocalInputPreferences(const float Sensitivity, const bool bInvertY,
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

void AKnightPlayerController::HandleJumpStarted()
{
	if (AKnightCharacter* Knight = GetControlledKnight(); Knight != nullptr && Knight->GetController() == this
		&& Knight->GetLifeComponent()->IsAlive() && !IsMoveInputIgnored()
		&& !Knight->GetKnightAbilitySystem()->HasMatchingGameplayTag(KnightGameplayTags::State_Combat_Staggered))
	{
		Knight->Jump();
	}
}

void AKnightPlayerController::HandleJumpEnded()
{
	if (AKnightCharacter* Knight = GetControlledKnight())
	{
		Knight->StopJumping();
	}
}

void AKnightPlayerController::HandleSelectKnight(int32 DesiredSlot)
{
	RequestSwitchKnight(DesiredSlot);
}

void AKnightPlayerController::HandleRestartDrill()
{
	RequestRestartDrill();
}

void AKnightPlayerController::HandleAbilityPressed(FGameplayTag InputTag)
{
	if (bToggleBlockInput && InputTag == KnightGameplayTags::Input_Combat_Block && GetControlledKnight() != nullptr)
	{
		RequestAbilityInput(InputTag, !GetControlledKnight()->GetCombatComponent()->GetSnapshot().bBlocking);
		return;
	}
	RequestAbilityInput(InputTag, true);
}

void AKnightPlayerController::HandleAbilityReleased(FGameplayTag InputTag)
{
	if (bToggleBlockInput && InputTag == KnightGameplayTags::Input_Combat_Block)
	{
		return;
	}
	if (bHoldLockInput && InputTag == KnightGameplayTags::Input_Combat_LockOn
		&& GetControlledKnight() != nullptr && GetControlledKnight()->GetCombatComponent()->GetLockedTarget() != nullptr)
	{
		RequestAbilityInput(InputTag, true);
		return;
	}
	RequestAbilityInput(InputTag, false);
}

void AKnightPlayerController::HandleInteract()
{
	RequestInteract();
}
