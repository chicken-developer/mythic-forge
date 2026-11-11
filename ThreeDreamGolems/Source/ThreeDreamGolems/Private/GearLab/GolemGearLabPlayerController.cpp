#include "GearLab/GolemGearLabPlayerController.h"

#include "Character/GolemCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GearLab/GolemGearLabGameMode.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Inventory/GolemInventoryComponent.h"
#include "Progression/GolemRPGComponent.h"
#include "RPGLoop/GolemRPGGameState.h"
#include "RPGLoop/GolemRPGStateActor.h"
#include "Widgets/GolemRPGPanel.h"

void AGolemGearLabPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (Input == nullptr || (BoundQuickInput.Get() == Input && !QuickHandles.IsEmpty()))
	{
		return;
	}
	if (BoundQuickInput.IsValid())
	{
		for (const uint32 Handle : QuickHandles)
		{
			BoundQuickInput->RemoveBindingByHandle(Handle);
		}
	}
	QuickHandles.Reset();
	BoundQuickInput = Input;
	if (QuickMappings == nullptr)
	{
		QuickMappings = NewObject<UInputMappingContext>(this, TEXT("GearQuickUse"));
		QuickModifier = NewObject<UInputAction>(this, TEXT("QuickUseModifier"));
		QuickModifier->ValueType = EInputActionValueType::Boolean;
		QuickMappings->MapKey(QuickModifier, EKeys::Gamepad_Special_Left);
		const TArray<FKey> Keys{EKeys::One, EKeys::Two};
		const TArray<FKey> PadKeys{EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Left};
		for (int32 Index = 0; Index < Keys.Num(); ++Index)
		{
			UInputAction* Action = NewObject<UInputAction>(this,
				FName(*FString::Printf(TEXT("QuickUse%d"), Index + 1)));
			Action->ValueType = EInputActionValueType::Boolean;
			QuickMappings->MapKey(Action, Keys[Index]);
			UInputTriggerChordAction* Chord = NewObject<UInputTriggerChordAction>(Action);
			Chord->ChordAction = QuickModifier;
			QuickMappings->MapKey(Action, PadKeys[Index]).Triggers.Add(Chord);
			QuickActions.Add(Action);
		}
	}
	for (int32 Index = 0; Index < QuickActions.Num(); ++Index)
	{
		QuickHandles.Add(Input->BindAction(QuickActions[Index], ETriggerEvent::Started,
			this, &ThisClass::RequestQuickUse, Index).GetHandle());
	}
	if (ULocalPlayer* Local = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->AddMappingContext(QuickMappings, GetGameplayMappingPriority() + 120);
			QuickSubsystem = Subsystem;
		}
	}
}

void AGolemGearLabPlayerController::RequestQuickUse(const int32 QuickSlot)
{
	if (GetRPGPanel() != nullptr && GetRPGPanel()->IsActivated())
	{
		return;
	}
	const AGolemRPGGameState* State = GetWorld()->GetGameState<AGolemRPGGameState>();
	const AGolemRPGStateActor* ConfirmedState = State != nullptr ? State->GetRPGState() : nullptr;
	AGolemCharacter* Golem = GetControlledGolem();
	if (ConfirmedState == nullptr || Golem == nullptr || QuickSlot < 0 || QuickSlot > 1)
	{
		return;
	}
	const FGolemInventoryView Inventory = ConfirmedState->GetInventory()->GetView();
	const FGolemRPGGolemView View = ConfirmedState->GetRPG()->GetGolemView(Golem->GetGolemEntityId());
	FGolemRPGCommand Request;
	Request.Operation = EGolemRPGOperation::UseConsumable;
	Request.RequestId = FGuid::NewGuid();
	Request.ExpectedGolemId = Golem->GetGolemEntityId();
	Request.InventoryId = Inventory.InventoryId;
	Request.ExpectedInventoryRevision = Inventory.Revision;
	Request.ExpectedGolemRevision = View.Revision;
	Request.QuickSlot = QuickSlot;
	RequestRPGCommand(Request);
}

void AGolemGearLabPlayerController::RequestGearFixture(const FName Action, const FGuid RequestId)
{
	if (AGolemCharacter* Golem = GetControlledGolem())
	{
		ServerRequestGearFixture(Action, RequestId, Golem->GetGolemEntityId());
	}
}

void AGolemGearLabPlayerController::ServerRequestGearFixture_Implementation(const FName Action,
	const FGuid RequestId, FGolemEntityId ExpectedGolemId)
{
	AGolemCharacter* Golem = GetControlledGolem();
	AGolemGearLabGameMode* Mode = GetWorld()->GetAuthGameMode<AGolemGearLabGameMode>();
	const FGolemResult Result = Mode != nullptr && Golem != nullptr
		&& Golem->GetGolemEntityId() == ExpectedGolemId && Golem->GetController() == this
		? Mode->ExecuteFixtureAction(this, Action, RequestId)
		: FGolemResult::Failure(EGolemError::NotOwned, TEXT("GearFixtureOwner"), FText::GetEmpty());
	ClientReceiveGearFixture(RequestId, Result);
}

void AGolemGearLabPlayerController::ClientReceiveGearFixture_Implementation(const FGuid RequestId, FGolemResult Result)
{
	OnGearFixtureResult.Broadcast(RequestId, Result);
}

void AGolemGearLabPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (BoundQuickInput.IsValid())
	{
		for (const uint32 Handle : QuickHandles)
		{
			BoundQuickInput->RemoveBindingByHandle(Handle);
		}
	}
	if (QuickSubsystem.IsValid())
	{
		QuickSubsystem->RemoveMappingContext(QuickMappings);
	}
	Super::EndPlay(Reason);
}
