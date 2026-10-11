#include "SkillLab/KnightSkillLabPlayerController.h"

#include "Character/KnightCharacter.h"
#include "Attributes/KnightAttributeSet.h"
#include "AbilityYard/KnightAbilityDummy.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Input/KnightInputConfig.h"
#include "InputTriggers.h"
#include "PartyArena/KnightArenaEnemy.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "Skills/KnightSkillTypes.h"
#include "Tags/KnightGameplayTags.h"
#include "Widgets/KnightRPGPanel.h"

void AKnightSkillLabPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
	if (Enhanced == nullptr || (BoundSkillInput.Get() == Enhanced && !BindingHandles.IsEmpty()))
	{
		return;
	}
	if (BoundSkillInput.IsValid())
	{
		for (const uint32 Handle : BindingHandles)
		{
			BoundSkillInput->RemoveBindingByHandle(Handle);
		}
	}
	BindingHandles.Reset();
	BoundSkillInput = Enhanced;
	if (SkillMappings == nullptr)
	{
		SkillMappings = NewObject<UInputMappingContext>(this, TEXT("SkillLabSlots"));
		SkillModifier = NewObject<UInputAction>(this, TEXT("SkillModifier"));
		SkillModifier->ValueType = EInputActionValueType::Boolean;
		SkillModifier->bConsumeInput = true;
		SkillMappings->MapKey(SkillModifier, EKeys::Gamepad_LeftTrigger);
		const TArray<FKey> Keys{EKeys::Z, EKeys::X, EKeys::C, EKeys::V};
		const TArray<FKey> PadKeys{EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Right,
			EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Left};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			UInputAction* Action = NewObject<UInputAction>(this,
				FName(*FString::Printf(TEXT("SkillSlot%d"), Index + 1)));
			Action->ValueType = EInputActionValueType::Boolean;
			Action->bConsumeInput = true;
			SkillActions.Add(Action);
			SkillMappings->MapKey(Action, Keys[Index]);
			UInputTriggerChordAction* Chord = NewObject<UInputTriggerChordAction>(Action);
			Chord->ChordAction = SkillModifier;
			SkillMappings->MapKey(Action, PadKeys[Index]).Triggers.Add(Chord);
		}
		// Z/C/V are now specialization slots in this map; keep the displaced M3/M4 actions accessible.
		if (const UKnightInputConfig* Config = GetKnightInputConfig())
		{
			if (const UInputAction* Parry = Config->FindAction(KnightGameplayTags::Input_Combat_Parry))
			{
				SkillMappings->MapKey(const_cast<UInputAction*>(Parry), EKeys::LeftAlt);
			}
			if (const UInputAction* Traverse = Config->FindAction(KnightGameplayTags::Input_World_Traverse))
			{
				SkillMappings->MapKey(const_cast<UInputAction*>(Traverse), EKeys::T);
			}
			if (const UInputAction* Heavy = Config->FindAction(KnightGameplayTags::Input_Combat_Heavy))
			{
				UInputTriggerChordAction* HeavyChord = NewObject<UInputTriggerChordAction>(SkillMappings);
				HeavyChord->ChordAction = SkillModifier;
				SkillMappings->MapKey(const_cast<UInputAction*>(Heavy), EKeys::Gamepad_RightTrigger).Triggers.Add(HeavyChord);
			}
		}
		HoldCommand = NewObject<UInputAction>(this, TEXT("SkillLabHoldCommand"));
		HoldCommand->ValueType = EInputActionValueType::Boolean;
		SkillMappings->MapKey(HoldCommand, EKeys::J);
	}
	for (int32 Index = 0; Index < SkillActions.Num(); ++Index)
	{
		BindingHandles.Add(Enhanced->BindAction(SkillActions[Index], ETriggerEvent::Started,
			this, &ThisClass::RequestSkillSlot, Index + 1).GetHandle());
	}
	BindingHandles.Add(Enhanced->BindAction(HoldCommand, ETriggerEvent::Started,
		this, &ThisClass::RequestHoldCommand).GetHandle());
	ULocalPlayer* Local = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = Local != nullptr
		? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Subsystem != nullptr && SkillSubsystem.Get() != Subsystem)
	{
		if (SkillSubsystem.IsValid())
		{
			SkillSubsystem->RemoveMappingContext(SkillMappings);
		}
		// Base combat uses GameplayMappingPriority +100; the lab overlay must win shared keys.
		Subsystem->AddMappingContext(SkillMappings, GetGameplayMappingPriority() + 110);
		SkillSubsystem = Subsystem;
	}
}

void AKnightSkillLabPlayerController::RequestHoldCommand()
{
	if (GetRPGPanel() == nullptr || !GetRPGPanel()->IsActivated())
	{
		RequestCompanionCommand(EKnightCompanionCommand::Hold);
	}
}

void AKnightSkillLabPlayerController::RequestSkillSlot(const int32 Slot)
{
	if (GetRPGPanel() != nullptr && GetRPGPanel()->IsActivated())
	{
		return;
	}
	AKnightCharacter* Knight = GetControlledKnight();
	if (Knight != nullptr)
	{
		ServerRequestSkillSlot(Slot, Knight->GetKnightEntityId());
	}
}

void AKnightSkillLabPlayerController::ServerRequestSkillSlot_Implementation(const int32 Slot,
	FKnightEntityId ExpectedKnightId)
{
	AKnightCharacter* Knight = GetControlledKnight();
	if (Knight == nullptr || Knight->GetController() != this || Knight->GetKnightEntityId() != ExpectedKnightId
		|| Slot < 1 || Slot > 4 || !Knight->GetLifeComponent()->IsAlive())
	{
		return;
	}
	UKnightSkillRuntimeComponent* Runtime = Knight->FindComponentByClass<UKnightSkillRuntimeComponent>();
	if (Runtime == nullptr)
	{
		return;
	}
	const FKnightSpecActionDefinition Definition = FKnightSkillCatalog::GetSkill(Runtime->GetBuild(), Slot);
	AActor* Target = nullptr;
	if (Definition.bFriendly)
	{
		float MissingHealth = -1.0f;
		for (TActorIterator<AKnightCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetLifeComponent()->IsAlive()
				&& FVector::DistSquared(It->GetActorLocation(), Knight->GetActorLocation()) <= FMath::Square(1200.0))
			{
				const UKnightAttributeSet* Attributes = It->GetKnightAttributeSet();
				const float Missing = Attributes->GetMaxHealth() - Attributes->GetHealth();
				if (Missing > MissingHealth)
				{
					MissingHealth = Missing;
					Target = *It;
				}
			}
		}
	}
	else
	{
		Target = Knight->GetCombatComponent()->GetLockedTarget();
		double BestDistance = FMath::Square(1500.0);
		if (Target == nullptr)
		{
			for (TActorIterator<AKnightArenaEnemy> It(GetWorld()); It; ++It)
			{
				const double Distance = FVector::DistSquared(It->GetActorLocation(), Knight->GetActorLocation());
				if (It->GetLifeComponent()->IsAlive() && Distance < BestDistance)
				{
					BestDistance = Distance;
					Target = *It;
				}
			}
			for (TActorIterator<AKnightAbilityDummy> It(GetWorld()); It; ++It)
			{
				const double Distance = FVector::DistSquared(It->GetActorLocation(), Knight->GetActorLocation());
				if (It->GetLifeComponent()->IsAlive() && Distance < BestDistance)
				{
					BestDistance = Distance;
					Target = *It;
				}
			}
		}
	}
	Runtime->RequestSkill(Slot, Target != nullptr ? Target : Knight);
}

void AKnightSkillLabPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundSkillInput.IsValid())
	{
		for (const uint32 Handle : BindingHandles)
		{
			BoundSkillInput->RemoveBindingByHandle(Handle);
		}
	}
	if (SkillSubsystem.IsValid())
	{
		SkillSubsystem->RemoveMappingContext(SkillMappings);
	}
	Super::EndPlay(EndPlayReason);
}
