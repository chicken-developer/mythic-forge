#include "GearLab/KnightGearLabPlayerController.h"

#include "Character/KnightCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GearLab/KnightGearLabGameMode.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Inventory/KnightInventoryComponent.h"
#include "Progression/KnightRPGComponent.h"
#include "RPGLoop/KnightRPGGameState.h"
#include "RPGLoop/KnightRPGStateActor.h"
#include "Widgets/KnightRPGPanel.h"

void AKnightGearLabPlayerController::SetupInputComponent()
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

void AKnightGearLabPlayerController::RequestQuickUse(const int32 QuickSlot)
{
	if (GetRPGPanel() != nullptr && GetRPGPanel()->IsActivated())
	{
		return;
	}
	const AKnightRPGGameState* State = GetWorld()->GetGameState<AKnightRPGGameState>();
	const AKnightRPGStateActor* ConfirmedState = State != nullptr ? State->GetRPGState() : nullptr;
	AKnightCharacter* Knight = GetControlledKnight();
	if (ConfirmedState == nullptr || Knight == nullptr || QuickSlot < 0 || QuickSlot > 1)
	{
		return;
	}
	const FKnightInventoryView Inventory = ConfirmedState->GetInventory()->GetView();
	const FKnightRPGKnightView View = ConfirmedState->GetRPG()->GetKnightView(Knight->GetKnightEntityId());
	FKnightRPGCommand Request;
	Request.Operation = EKnightRPGOperation::UseConsumable;
	Request.RequestId = FGuid::NewGuid();
	Request.ExpectedKnightId = Knight->GetKnightEntityId();
	Request.InventoryId = Inventory.InventoryId;
	Request.ExpectedInventoryRevision = Inventory.Revision;
	Request.ExpectedKnightRevision = View.Revision;
	Request.QuickSlot = QuickSlot;
	RequestRPGCommand(Request);
}

void AKnightGearLabPlayerController::RequestGearFixture(const FName Action, const FGuid RequestId)
{
	if (AKnightCharacter* Knight = GetControlledKnight())
	{
		ServerRequestGearFixture(Action, RequestId, Knight->GetKnightEntityId());
	}
}

void AKnightGearLabPlayerController::ServerRequestGearFixture_Implementation(const FName Action,
	const FGuid RequestId, FKnightEntityId ExpectedKnightId)
{
	AKnightCharacter* Knight = GetControlledKnight();
	AKnightGearLabGameMode* Mode = GetWorld()->GetAuthGameMode<AKnightGearLabGameMode>();
	const FKnightResult Result = Mode != nullptr && Knight != nullptr
		&& Knight->GetKnightEntityId() == ExpectedKnightId && Knight->GetController() == this
		? Mode->ExecuteFixtureAction(this, Action, RequestId)
		: FKnightResult::Failure(EKnightError::NotOwned, TEXT("GearFixtureOwner"), FText::GetEmpty());
	ClientReceiveGearFixture(RequestId, Result);
}

void AKnightGearLabPlayerController::ClientReceiveGearFixture_Implementation(const FGuid RequestId, FKnightResult Result)
{
	OnGearFixtureResult.Broadcast(RequestId, Result);
}

void AKnightGearLabPlayerController::EndPlay(const EEndPlayReason::Type Reason)
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
