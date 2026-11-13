#include "Input/GolemInputConfig.h"

#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Tags/GolemGameplayTags.h"

void UGolemInputConfig::InitializeDefaultMapping()
{
	if (MappingContext != nullptr)
	{
		return;
	}
	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Golem_Gameplay"));
	UInputAction* MoveAction = CreateAction(GolemGameplayTags::Input_Move,
		EInputActionValueType::Axis2D, TEXT("IA_Golem_Move"));
	MoveAction->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;
	MapAxis(MoveAction, EKeys::W, true, false);
	MapAxis(MoveAction, EKeys::S, true, true);
	MapAxis(MoveAction, EKeys::D, false, false);
	MapAxis(MoveAction, EKeys::A, false, true);
	FEnhancedActionKeyMapping& GamepadMove = MappingContext->MapKey(MoveAction, EKeys::Gamepad_Left2D);
	UInputModifierDeadZone* MoveDeadZone = NewObject<UInputModifierDeadZone>(MappingContext);
	MoveDeadZone->LowerThreshold = GamepadDeadZone;
	GamepadMove.Modifiers.Add(MoveDeadZone);

	UInputAction* LookAction = CreateAction(GolemGameplayTags::Input_Look,
		EInputActionValueType::Axis2D, TEXT("IA_Golem_Look"));
	LookAction->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;
	MapAxis(LookAction, EKeys::MouseX, false, false);
	MapAxis(LookAction, EKeys::MouseY, true, true);
	FEnhancedActionKeyMapping& GamepadLook = MappingContext->MapKey(LookAction, EKeys::Gamepad_Right2D);
	UInputModifierDeadZone* LookDeadZone = NewObject<UInputModifierDeadZone>(MappingContext);
	LookDeadZone->LowerThreshold = GamepadDeadZone;
	GamepadLook.Modifiers.Add(LookDeadZone);
	UInputModifierScalar* LookRate = NewObject<UInputModifierScalar>(MappingContext);
	LookRate->Scalar = FVector(GamepadLookRate, -GamepadLookRate, 1.0f);
	GamepadLook.Modifiers.Add(LookRate);
	GamepadLook.Modifiers.Add(NewObject<UInputModifierScaleByDeltaTime>(MappingContext));

	UInputAction* JumpAction = CreateAction(GolemGameplayTags::Input_Jump,
		EInputActionValueType::Boolean, TEXT("IA_Golem_Jump"));
	MappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	MappingContext->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	UInputAction* NextAction = CreateAction(GolemGameplayTags::Input_Golem_Next,
		EInputActionValueType::Boolean, TEXT("IA_Golem_Next"));
	MappingContext->MapKey(NextAction, EKeys::Tab);
	MappingContext->MapKey(NextAction, EKeys::Gamepad_RightShoulder);
	UInputAction* FirstAction = CreateAction(GolemGameplayTags::Input_Golem_Select_First,
		EInputActionValueType::Boolean, TEXT("IA_Golem_SelectFirst"));
	MappingContext->MapKey(FirstAction, EKeys::One);
	UInputAction* SecondAction = CreateAction(GolemGameplayTags::Input_Golem_Select_Second,
		EInputActionValueType::Boolean, TEXT("IA_Golem_SelectSecond"));
	MappingContext->MapKey(SecondAction, EKeys::Two);
	UInputAction* ThirdAction = CreateAction(GolemGameplayTags::Input_Golem_Select_Third,
		EInputActionValueType::Boolean, TEXT("IA_Golem_SelectThird"));
	MappingContext->MapKey(ThirdAction, EKeys::Three);
	UInputAction* RetryAction = CreateAction(GolemGameplayTags::Input_Training_Retry,
		EInputActionValueType::Boolean, TEXT("IA_Golem_Retry"));
	MappingContext->MapKey(RetryAction, EKeys::R);
	MappingContext->MapKey(RetryAction, EKeys::Gamepad_FaceButton_Top);
	UInputAction* TestAction = CreateAction(GolemGameplayTags::Input_Ability_Test,
		EInputActionValueType::Boolean, TEXT("IA_Golem_TestAbility"));
	MappingContext->MapKey(TestAction, EKeys::T);
	MappingContext->MapKey(TestAction, EKeys::Gamepad_RightTrigger);
	UInputAction* ReviveAction = CreateAction(GolemGameplayTags::Input_Party_Revive,
		EInputActionValueType::Boolean, TEXT("IA_Golem_Revive"));
	MappingContext->MapKey(ReviveAction, EKeys::E);
	MappingContext->MapKey(ReviveAction, EKeys::Gamepad_FaceButton_Left);
	UInputAction* InteractAction = CreateAction(GolemGameplayTags::Input_World_Interact,
		EInputActionValueType::Boolean, TEXT("IA_Golem_Interact"));
	MappingContext->MapKey(InteractAction, EKeys::F);
	MappingContext->MapKey(InteractAction, EKeys::Gamepad_LeftShoulder);
	CombatMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Golem_Combat"));
	CombatMappingContext->MapKey(InteractAction, EKeys::Gamepad_DPad_Left);
	struct FGolemCombatBinding
	{
		FGameplayTag Tag;
		FName Name;
		FKey Desktop;
		FKey Gamepad;
	};
	const TArray<FGolemCombatBinding> CombatBindings =
	{
		{GolemGameplayTags::Input_Combat_Primary, TEXT("IA_Golem_Light"), EKeys::LeftMouseButton, EKeys::Gamepad_RightTrigger},
		{GolemGameplayTags::Input_Combat_Heavy, TEXT("IA_Golem_Heavy"), EKeys::RightMouseButton, EKeys::Gamepad_LeftTrigger},
		{GolemGameplayTags::Input_Combat_Block, TEXT("IA_Golem_Block"), EKeys::Q, EKeys::Gamepad_LeftShoulder},
		{GolemGameplayTags::Input_Combat_Dodge, TEXT("IA_Golem_Dodge"), EKeys::LeftShift, EKeys::Gamepad_FaceButton_Right}
	};
	for (const FGolemCombatBinding& Binding : CombatBindings)
	{
		UInputAction* Action = CreateAction(Binding.Tag, EInputActionValueType::Boolean, Binding.Name);
		Action->bConsumeInput = true;
		CombatMappingContext->MapKey(Action, Binding.Desktop);
		CombatMappingContext->MapKey(Action, Binding.Gamepad);
		CombatInputTags.Add(Binding.Tag);
	}
	OriginalGameplayMappings = MappingContext->GetMappings();
	OriginalCombatMappings = CombatMappingContext->GetMappings();
}

FGolemResult UGolemInputConfig::ApplyDesktopRemapping(const TMap<FKey, FKey>& Remapping)
{
	InitializeDefaultMapping();
	TSet<FKey> Used;
	for (const TPair<FKey, FKey>& Pair : Remapping)
	{
		if (!Pair.Key.IsValid() || !Pair.Value.IsValid() || Pair.Key.IsGamepadKey()
			|| Pair.Value.IsGamepadKey() || Pair.Value.IsAnalog() || Used.Contains(Pair.Value))
		{
			return FGolemResult::Failure(EGolemError::Conflict, TEXT("InputRemap"), FText::GetEmpty());
		}
		Used.Add(Pair.Value);
	}
	const auto Rebuild = [&Remapping](UInputMappingContext* Context, const TArray<FEnhancedActionKeyMapping>& Original)
	{
		Context->UnmapAll();
		for (const FEnhancedActionKeyMapping& Source : Original)
		{
			const FKey* Key = Remapping.Find(Source.Key);
			FEnhancedActionKeyMapping& Mapping = Context->MapKey(Source.Action, Key != nullptr ? *Key : Source.Key);
			Mapping.Modifiers = Source.Modifiers;
			Mapping.Triggers = Source.Triggers;
		}
	};
	Rebuild(MappingContext, OriginalGameplayMappings);
	Rebuild(CombatMappingContext, OriginalCombatMappings);
	return FGolemResult::Success();
}

const UInputAction* UGolemInputConfig::FindAction(const FGameplayTag& InputTag) const
{
	const TObjectPtr<UInputAction>* Action = Actions.Find(InputTag);
	return Action != nullptr ? Action->Get() : nullptr;
}

UInputMappingContext* UGolemInputConfig::GetMappingContext() const
{
	return MappingContext;
}

UInputMappingContext* UGolemInputConfig::GetCombatMappingContext() const
{
	return CombatMappingContext;
}

bool UGolemInputConfig::IsCombatInput(const FGameplayTag& InputTag) const
{
	return CombatInputTags.Contains(InputTag);
}

UInputAction* UGolemInputConfig::CreateAction(
	const FGameplayTag& InputTag, EInputActionValueType ValueType, FName ActionName)
{
	UInputAction* Action = NewObject<UInputAction>(this, ActionName);
	Action->ValueType = ValueType;
	Actions.Add(InputTag, Action);
	return Action;
}

void UGolemInputConfig::MapAxis(UInputAction* Action, const FKey& Key, bool bIsVertical, bool bIsNegative)
{
	FEnhancedActionKeyMapping& Mapping = MappingContext->MapKey(Action, Key);
	if (bIsNegative)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));
	}
	if (bIsVertical)
	{
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(MappingContext);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
	}
}
