#include "Campaign/KnightCampaignPlayerController.h"

#include "Character/KnightCharacter.h"
#include "Components/KnightCombatComponent.h"
#include "Components/KnightLifeComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Input/KnightInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Settings/KnightGameUserSettings.h"
#include "Skills/KnightSkillRuntimeComponent.h"
#include "Tags/KnightGameplayTags.h"
#include "Widgets/KnightRPGPanel.h"

void AKnightCampaignPlayerController::InstallCampaignSkillInput()
{
	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
	if (Enhanced == nullptr || CampaignSkillInput.Get() == Enhanced && !CampaignSkillBindings.IsEmpty())
	{
		return;
	}
	RemoveCampaignSkillInput();
	CampaignSkillInput = Enhanced;
	if (CampaignSkillMappings == nullptr)
	{
		CampaignSkillMappings = NewObject<UInputMappingContext>(this, TEXT("CampaignSkillSlots"));
		CampaignSkillModifier = NewObject<UInputAction>(this, TEXT("CampaignSkillModifier"));
		CampaignSkillModifier->ValueType = EInputActionValueType::Boolean;
		CampaignSkillModifier->bConsumeInput = true;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			UInputAction* Action = NewObject<UInputAction>(this,
				FName(*FString::Printf(TEXT("CampaignSkill%d"), Index + 1)));
			Action->ValueType = EInputActionValueType::Boolean;
			Action->bConsumeInput = true;
			CampaignSkillActions.Add(Action);
		}
	}
	for (int32 Index = 0; Index < CampaignSkillActions.Num(); ++Index)
	{
		CampaignSkillBindings.Add(Enhanced->BindAction(CampaignSkillActions[Index], ETriggerEvent::Started,
			this, &ThisClass::RequestCampaignSkill, Index + 1).GetHandle());
	}
	RefreshCampaignSkillMappings();
}

void AKnightCampaignPlayerController::RefreshCampaignSkillMappings()
{
	if (CampaignSkillMappings == nullptr || CampaignSkillActions.Num() != 4)
	{
		return;
	}
	CampaignSkillMappings->UnmapAll();
	CampaignSkillMappings->MapKey(CampaignSkillModifier, EKeys::Gamepad_LeftTrigger);
	const FKey Defaults[]{EKeys::F1, EKeys::F2, EKeys::F3, EKeys::F4};
	const FKey Pad[]{EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Right,
		EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Left};
	UKnightGameUserSettings* Settings = UKnightGameUserSettings::Get();
	for (int32 Index = 0; Index < CampaignSkillActions.Num(); ++Index)
	{
		const FName ActionId(*FString::Printf(TEXT("Skill%d"), Index + 1));
		const FKey Key = Settings != nullptr ? Settings->GetActionKey(ActionId) : Defaults[Index];
		CampaignSkillMappings->MapKey(CampaignSkillActions[Index], Key);
		UInputTriggerChordAction* Chord = NewObject<UInputTriggerChordAction>(CampaignSkillMappings);
		Chord->ChordAction = CampaignSkillModifier;
		CampaignSkillMappings->MapKey(CampaignSkillActions[Index], Pad[Index]).Triggers.Add(Chord);
	}
	if (const UKnightInputConfig* Config = GetKnightInputConfig())
	{
		if (const UInputAction* Heavy = Config->FindAction(KnightGameplayTags::Input_Combat_Heavy))
		{
			UInputTriggerChordAction* Chord = NewObject<UInputTriggerChordAction>(CampaignSkillMappings);
			Chord->ChordAction = CampaignSkillModifier;
			CampaignSkillMappings->MapKey(const_cast<UInputAction*>(Heavy), EKeys::Gamepad_RightTrigger).Triggers.Add(Chord);
		}
	}
	ULocalPlayer* Local = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Input = Local != nullptr
		? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Input != nullptr)
	{
		Input->AddMappingContext(CampaignSkillMappings, GetGameplayMappingPriority() + 110);
		Input->RequestRebuildControlMappings();
		CampaignSkillSubsystem = Input;
	}
}

void AKnightCampaignPlayerController::RequestCampaignSkill(const int32 Slot)
{
	if (bFrontEnd || GetRPGPanel() != nullptr && GetRPGPanel()->IsActivated())
	{
		return;
	}
	if (AKnightCharacter* Knight = GetControlledKnight())
	{
		ServerRequestCampaignSkill(Slot, Knight->GetKnightEntityId());
	}
}

void AKnightCampaignPlayerController::ServerRequestCampaignSkill_Implementation(const int32 Slot,
	const FKnightEntityId ExpectedKnight)
{
	AKnightCharacter* Knight = GetControlledKnight();
	FKnightResult Result = FKnightResult::Failure(EKnightError::NotOwned, TEXT("SkillCaller"), FText::GetEmpty());
	if (Knight != nullptr && Knight->GetController() == this && Knight->GetKnightEntityId() == ExpectedKnight
		&& Knight->GetLifeComponent()->IsAlive() && Slot >= 1 && Slot <= 4)
	{
		if (UKnightSkillRuntimeComponent* Runtime = Knight->FindComponentByClass<UKnightSkillRuntimeComponent>())
		{
			const FKnightSpecActionDefinition Definition = FKnightSkillCatalog::GetSkill(Runtime->GetBuild(), Slot);
			AActor* Target = Definition.bFriendly ? Runtime->SelectLowestHealthAlly(Definition.Range)
				: Knight->GetCombatComponent()->GetLockedTarget();
			if (!Definition.bFriendly && Target == nullptr)
			{
				Target = Knight->GetCombatComponent()->FindCombatTarget(Definition.Range);
			}
			Result = Runtime->RequestSkill(Slot, Target);
		}
	}
	ClientReceiveCampaignSkillResult(Result);
}

void AKnightCampaignPlayerController::ClientReceiveCampaignSkillResult_Implementation(const FKnightResult Result)
{
	LastCampaignResult = Result;
	if (!Result.IsSuccess())
	{
		Presentation.Feedback = !Result.Message.IsEmpty() ? Result.Message : FText::FromName(Result.Context);
		RefreshCampaignPresentation();
	}
}

void AKnightCampaignPlayerController::RemoveCampaignSkillInput()
{
	if (CampaignSkillInput.IsValid())
	{
		for (const uint32 Handle : CampaignSkillBindings)
		{
			CampaignSkillInput->RemoveBindingByHandle(Handle);
		}
	}
	CampaignSkillBindings.Reset();
	CampaignSkillInput.Reset();
	if (CampaignSkillSubsystem.IsValid())
	{
		CampaignSkillSubsystem->RemoveMappingContext(CampaignSkillMappings);
	}
	CampaignSkillSubsystem.Reset();
}
