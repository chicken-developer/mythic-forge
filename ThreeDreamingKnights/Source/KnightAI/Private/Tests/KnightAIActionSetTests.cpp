#include "Definitions/KnightAIActionSetDefinition.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tags/KnightGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace KnightAIActionSetTests
{
	FKnightAIActionEntry MakeAction(FName Key, const FGameplayTag& Tag, int32 Priority)
	{
		FKnightAIActionEntry Action;
		Action.ActionKey = Key;
		Action.ActionTag = Tag;
		Action.MinRange = 100.0f;
		Action.MaxRange = 300.0f;
		Action.RequiredStamina = 20.0f;
		Action.RequiredRuneEnergy = 10.0f;
		Action.CooldownSeconds = 2.0f;
		Action.Priority = Priority;
		Action.RequiredStateTags.AddTag(KnightGameplayTags::State_Life_Alive);
		Action.BlockedStateTags.AddTag(KnightGameplayTags::State_Life_Downed);
		Action.BlockedStateTags.AddTag(KnightGameplayTags::State_Life_Dead);
		Action.BlockedStateTags.AddTag(KnightGameplayTags::State_Life_Reviving);
		return Action;
	}
	FKnightAIActionSelectionContext MakeContext()
	{
		FKnightAIActionSelectionContext Context;
		Context.Distance = 200.0f;
		Context.Stamina = 50.0f;
		Context.RuneEnergy = 25.0f;
		Context.OwnedAbilityActionTags.AddTag(KnightGameplayTags::Ability_Test_Pulse);
		Context.OwnedAbilityActionTags.AddTag(KnightGameplayTags::Ability_Party_Revive);
		Context.StateTags.AddTag(KnightGameplayTags::State_Life_Alive);
		return Context;
	}
	bool ContextsEqual(const FKnightAIActionSelectionContext& Left, const FKnightAIActionSelectionContext& Right)
	{
		return Left.Distance == Right.Distance && Left.Stamina == Right.Stamina && Left.RuneEnergy == Right.RuneEnergy &&
			Left.OwnedAbilityActionTags == Right.OwnedAbilityActionTags && Left.StateTags == Right.StateTags &&
			Left.RemainingCooldowns.OrderIndependentCompareEqual(Right.RemainingCooldowns);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAIActionSetValidationTest, "Knight.AI.ActionSets.DefinitionContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAIActionSetValidationTest::RunTest(const FString& Parameters)
{
	using namespace KnightAIActionSetTests;
	TStrongObjectPtr<UKnightAIActionSetDefinition> Definition(NewObject<UKnightAIActionSetDefinition>());
	TestEqual(TEXT("Action sets retain their stable primary type"), Definition->GetDefinitionType(), FName(TEXT("KnightAIActionSet")));
	TestEqual(TEXT("Unassigned identity is rejected"), Definition->ValidateActionSet().Error, EKnightError::InvalidId);
	TestTrue(TEXT("Native authoring assigns a stable identity"),
		Definition->TrySetIdentity(TEXT("FoundationActions"), TEXT("KnightAIActionSet")));
	const FKnightAIActionEntry Pulse = MakeAction(TEXT("Pulse"), KnightGameplayTags::Ability_Test_Pulse, 10);
	const FKnightAIActionEntry Revive = MakeAction(TEXT("Revive"), KnightGameplayTags::Ability_Party_Revive, 20);
	TestTrue(TEXT("A valid bounded action set can be authored"), Definition->TrySetActions({Pulse, Revive}).IsSuccess());
	TestTrue(TEXT("Authored action data and identity validate together"), Definition->ValidateActionSet().IsSuccess());
	TestTrue(TEXT("Primary identity is independent of the UObject name"),
		Definition->GetPrimaryAssetId() == FPrimaryAssetId(TEXT("KnightAIActionSet"), TEXT("FoundationActions")));
	TArray<FKnightAIActionEntry> Invalid = {Pulse, Revive};
	Invalid[1].ActionKey = Pulse.ActionKey;
	TestEqual(TEXT("Duplicate stable action keys are rejected atomically"),
		Definition->TrySetActions(Invalid).Error, EKnightError::DuplicateId);
	TestEqual(TEXT("Rejected duplicate keeps both previous actions"), Definition->GetActions().Num(), 2);
	TestEqual(TEXT("Rejected duplicate preserves the original second key"), Definition->GetActions()[1].ActionKey, Revive.ActionKey);
	Invalid = {Pulse, Revive};
	Invalid[0].RequiredStamina = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("Nonfinite resource tuning is rejected"), Definition->TrySetActions(Invalid).Error, EKnightError::InvalidData);
	TestEqual(TEXT("Nonfinite tuning did not partially overwrite the first action"),
		Definition->GetActions()[0].RequiredStamina, Pulse.RequiredStamina);
	Invalid = {Pulse, Revive};
	Invalid[0].ActionKey = TEXT("Bad-Key");
	TestEqual(TEXT("Unstable action keys are rejected"), Definition->TrySetActions(Invalid).Error, EKnightError::InvalidData);
	Invalid = {Pulse, Revive};
	Invalid[0].ActionTag = KnightGameplayTags::Input_Ability_Test;
	TestEqual(TEXT("An input tag cannot masquerade as a GAS action"),
		Definition->TrySetActions(Invalid).Error, EKnightError::InvalidData);
	Invalid = {Pulse, Revive};
	Invalid[0].MaxRange = 50.0f;
	TestEqual(TEXT("Reversed range bounds are rejected"), Definition->TrySetActions(Invalid).Error, EKnightError::InvalidData);
	Invalid = {Pulse, Revive};
	Invalid[0].CooldownSeconds = -1.0f;
	TestEqual(TEXT("Negative cooldown tuning is rejected"), Definition->TrySetActions(Invalid).Error, EKnightError::InvalidData);
	Invalid = {Pulse, Revive};
	Invalid[0].BlockedStateTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Life")));
	TestEqual(TEXT("A blocked parent cannot contradict a required child state"),
		Definition->TrySetActions(Invalid).Error, EKnightError::InvalidData);
	TestEqual(TEXT("Empty action sets are rejected"), Definition->TrySetActions({}).Error, EKnightError::InvalidData);
	Invalid.Init(Pulse, 65);
	TestEqual(TEXT("Excessive action counts fail the bound before changing data"),
		Definition->TrySetActions(Invalid).Error, EKnightError::InvalidData);
	TestTrue(TEXT("Original set remains usable after all authoring failures"), Definition->ValidateActionSet().IsSuccess());
	TestTrue(TEXT("Stable identity setter still permits generic definition authoring"),
		Definition->TrySetIdentity(TEXT("WrongType"), TEXT("KnightDefinition")));
	TestEqual(TEXT("Action-set validation rejects a foreign primary asset type"),
		Definition->ValidateActionSet().Error, EKnightError::TypeMismatch);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAIActionEligibilityTest, "Knight.AI.ActionSets.Eligibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAIActionEligibilityTest::RunTest(const FString& Parameters)
{
	using namespace KnightAIActionSetTests;
	TStrongObjectPtr<UKnightAIActionSetDefinition> Definition(NewObject<UKnightAIActionSetDefinition>());
	Definition->TrySetIdentity(TEXT("EligibilityActions"), TEXT("KnightAIActionSet"));
	const FKnightAIActionEntry Action = MakeAction(TEXT("Pulse"), KnightGameplayTags::Ability_Test_Pulse, 10);
	if (!TestTrue(TEXT("Eligibility fixture is actual validated definition data"), Definition->TrySetActions({Action}).IsSuccess()))
	{
		return false;
	}
	FKnightAIActionSelectionContext Context = MakeContext();
	FKnightAIActionEntry Selected;
	TestTrue(TEXT("An eligible action is selected from explicit data"), Definition->TrySelectAction(Context, Selected).IsSuccess());
	TestEqual(TEXT("Selection returns the stable action key"), Selected.ActionKey, Action.ActionKey);
	Context.Distance = Action.MinRange;
	TestTrue(TEXT("Minimum range boundary is inclusive"), Definition->TrySelectAction(Context, Selected).IsSuccess());
	Context.Distance = Action.MaxRange;
	TestTrue(TEXT("Maximum range boundary is inclusive"), Definition->TrySelectAction(Context, Selected).IsSuccess());
	Context.Distance = Action.MaxRange + 1.0f;
	TestEqual(TEXT("Out-of-range snapshot has no eligible action"), Definition->TrySelectAction(Context, Selected).Error, EKnightError::NotFound);
	TestTrue(TEXT("No-eligible result clears the previous selection"), Selected.ActionKey.IsNone() && !Selected.ActionTag.IsValid());
	Context = MakeContext();
	Context.Stamina = Action.RequiredStamina - 1.0f;
	TestEqual(TEXT("Insufficient stamina prevents selection"), Definition->TrySelectAction(Context, Selected).Error, EKnightError::NotFound);
	Context = MakeContext();
	Context.RuneEnergy = Action.RequiredRuneEnergy - 1.0f;
	TestEqual(TEXT("Insufficient rune energy prevents selection"), Definition->TrySelectAction(Context, Selected).Error, EKnightError::NotFound);
	Context = MakeContext();
	Context.RemainingCooldowns.Add(Action.ActionKey, 0.01f);
	const FKnightAIActionSelectionContext CoolingSnapshot = Context;
	TestEqual(TEXT("Remaining cooldown prevents selection"), Definition->TrySelectAction(Context, Selected).Error, EKnightError::NotFound);
	TestTrue(TEXT("Failed selection never decreases the supplied cooldown"), ContextsEqual(Context, CoolingSnapshot));
	Context.RemainingCooldowns[Action.ActionKey] = 0.0f;
	TestTrue(TEXT("Zero remaining cooldown is eligible"), Definition->TrySelectAction(Context, Selected).IsSuccess());
	Context = MakeContext();
	Context.StateTags.RemoveTag(KnightGameplayTags::State_Life_Alive);
	TestEqual(TEXT("Missing required state prevents selection"), Definition->TrySelectAction(Context, Selected).Error, EKnightError::NotFound);
	Context = MakeContext();
	Context.StateTags.AddTag(KnightGameplayTags::State_Life_Reviving);
	TestEqual(TEXT("Blocked channel state prevents selection"), Definition->TrySelectAction(Context, Selected).Error, EKnightError::NotFound);
	Context = MakeContext();
	Context.OwnedAbilityActionTags.Reset();
	Context.OwnedAbilityActionTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Test")));
	TestEqual(TEXT("An owned family tag cannot grant a specific action"),
		Definition->TrySelectAction(Context, Selected).Error, EKnightError::NotFound);
	Context = MakeContext();
	Context.Distance = std::numeric_limits<float>::quiet_NaN();
	TestEqual(TEXT("Nonfinite distance is invalid data rather than eligibility"),
		Definition->TrySelectAction(Context, Selected).Error, EKnightError::InvalidData);
	Context = MakeContext();
	Context.Stamina = -1.0f;
	TestEqual(TEXT("Negative resource snapshots are invalid"), Definition->TrySelectAction(Context, Selected).Error, EKnightError::InvalidData);
	Context = MakeContext();
	Context.RemainingCooldowns.Add(Action.ActionKey, std::numeric_limits<float>::infinity());
	TestEqual(TEXT("Nonfinite cooldown snapshots are rejected"), Definition->TrySelectAction(Context, Selected).Error, EKnightError::InvalidData);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightAIActionSelectionTest, "Knight.AI.ActionSets.DeterministicSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightAIActionSelectionTest::RunTest(const FString& Parameters)
{
	using namespace KnightAIActionSetTests;
	TStrongObjectPtr<UKnightAIActionSetDefinition> Definition(NewObject<UKnightAIActionSetDefinition>());
	Definition->TrySetIdentity(TEXT("SelectionActions"), TEXT("KnightAIActionSet"));
	const FKnightAIActionEntry Zulu = MakeAction(TEXT("ZuluPulse"), KnightGameplayTags::Ability_Test_Pulse, 10);
	const FKnightAIActionEntry Alpha = MakeAction(TEXT("AlphaPulse"), KnightGameplayTags::Ability_Test_Pulse, 10);
	const FKnightAIActionEntry Revive = MakeAction(TEXT("Revive"), KnightGameplayTags::Ability_Party_Revive, 20);
	if (!TestTrue(TEXT("Selection fixture validates reused GAS action tags and distinct keys"),
		Definition->TrySetActions({Zulu, Alpha, Revive}).IsSuccess()))
	{
		return false;
	}
	FKnightAIActionSelectionContext Context = MakeContext();
	Context.RemainingCooldowns.Add(TEXT("UnusedAction"), 4.0f);
	FKnightAIActionEntry Selected;
	TestTrue(TEXT("Highest-priority eligible action is selected"), Definition->TrySelectAction(Context, Selected).IsSuccess());
	TestEqual(TEXT("Revive priority outranks eligible pulses"), Selected.ActionKey, Revive.ActionKey);
	Context.RemainingCooldowns.Add(Revive.ActionKey, 2.0f);
	const FKnightAIActionSelectionContext BeforeSelection = Context;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		TestTrue(TEXT("Repeated pure selection succeeds"), Definition->TrySelectAction(Context, Selected).IsSuccess());
		TestEqual(TEXT("Tie selects the same lexical stable key"), Selected.ActionKey, Alpha.ActionKey);
	}
	TestTrue(TEXT("Selection preserves every input scalar, tag container and cooldown"), ContextsEqual(Context, BeforeSelection));
	TestEqual(TEXT("Selection preserves the authored order"), Definition->GetActions()[0].ActionKey, Zulu.ActionKey);
	TestEqual(TEXT("Selection returns tuning without spending resources"), Selected.RequiredStamina, Alpha.RequiredStamina);
	TestTrue(TEXT("Authoring order can be reversed without changing identity"), Definition->TrySetActions({Revive, Alpha, Zulu}).IsSuccess());
	TestTrue(TEXT("Reordered set still selects an eligible action"), Definition->TrySelectAction(Context, Selected).IsSuccess());
	TestEqual(TEXT("Deterministic tie is independent of authoring order"), Selected.ActionKey, Alpha.ActionKey);
	Context.RemainingCooldowns.Add(Alpha.ActionKey, 1.0f);
	TestTrue(TEXT("Next eligible same-priority action is selected when first is cooling"),
		Definition->TrySelectAction(Context, Selected).IsSuccess());
	TestEqual(TEXT("Cooling higher tie leaves Zulu as eligible alternative"), Selected.ActionKey, Zulu.ActionKey);
	Context.RemainingCooldowns.Add(Zulu.ActionKey, 1.0f);
	const FKnightAIActionSelectionContext AllCooling = Context;
	TestEqual(TEXT("All cooling actions return typed no-eligible result"),
		Definition->TrySelectAction(Context, Selected).Error, EKnightError::NotFound);
	TestTrue(TEXT("No-eligible query retains the entire explicit context"), ContextsEqual(Context, AllCooling));
	return true;
}

#endif
