#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Assets/GolemAssetManager.h"
#include "ColorArena/GolemColorArenaGameMode.h"
#include "Definitions/GolemCombatProfileDefinition.h"
#include "Definitions/GolemHeroDefinition.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/PackageName.h"
#include "Tags/GolemGameplayTags.h"
#include "Types/GolemElement.h"
#include "UObject/Package.h"
#include "Xenon/GolemXenonDefinition.h"

namespace GolemColorArenaTests
{
	template <typename DefinitionType>
	DefinitionType* Resolve(FAutomationTestBase& Test, const TCHAR* Type, const TCHAR* Name)
	{
		UGolemAssetManager* Manager = UGolemAssetManager::GetGolemAssetManager();
		UGolemDefinition* Definition = nullptr;
		if (Manager == nullptr || !Manager->ValidateDefinitionCatalog().IsSuccess()
			|| !Manager->ResolveDefinition(FPrimaryAssetId(Type, Name), DefinitionType::StaticClass(), Definition).IsSuccess())
		{
			Test.AddError(FString::Printf(TEXT("%s:%s does not resolve through the asset manager."), Type, Name));
			return nullptr;
		}
		return Cast<DefinitionType>(Definition);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGolemColorArenaContentTest, "Golem.Color.Arena.AuthoredContent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGolemColorArenaContentTest::RunTest(const FString& Parameters)
{
	using namespace GolemColorArenaTests;
	const UGolemXenonDefinition* Grunt = Resolve<UGolemXenonDefinition>(*this, TEXT("GolemXenon"), TEXT("Grunt"));
	const UGolemXenonDefinition* Shield = Resolve<UGolemXenonDefinition>(*this, TEXT("GolemXenon"), TEXT("Shield"));
	if (Grunt == nullptr || Shield == nullptr)
	{
		return false;
	}
	TestTrue(TEXT("Grunt asset is valid"), Grunt->ValidateXenon().IsSuccess());
	TestEqual(TEXT("Grunt health"), Grunt->GetMaxHealth(), 300.0f);
	TestEqual(TEXT("Grunt damage"), Grunt->GetContactDamage(), 20.0f);
	TestEqual(TEXT("Grunt fills"), Grunt->GetVoidTuning().VoidsToFill, 3);
	TestEqual(TEXT("Grunt shows one void"), Grunt->GetVoidTuning().MaxOpenVoids, 1);
	TestTrue(TEXT("Grunt starts with a basic colour"), Grunt->IsFirstVoidBasic());
	TestTrue(TEXT("Shield asset is valid"), Shield->ValidateXenon().IsSuccess());
	TestEqual(TEXT("Shield health"), Shield->GetMaxHealth(), 400.0f);
	TestEqual(TEXT("Shield damage"), Shield->GetContactDamage(), 25.0f);
	TestEqual(TEXT("Shield fills"), Shield->GetVoidTuning().VoidsToFill, 4);
	TestTrue(TEXT("Shield voids sit front and back"), Shield->GetVoidSlots().Num() == 2
		&& Shield->GetVoidSlots()[0].Offset.X > 0.0 && Shield->GetVoidSlots()[1].Offset.X < 0.0);
	for (const UGolemXenonDefinition* Xenon : { Grunt, Shield })
	{
		TestEqual(TEXT("Grey body share"), Xenon->GetVoidTuning().GrayBodyDamageFraction, 0.1f);
		TestEqual(TEXT("Stagger"), Xenon->GetVoidTuning().StaggerSeconds, 2.5f);
		TestEqual(TEXT("Mixing window"), Xenon->GetVoidTuning().Void.MixWindowSeconds, 4.0f);
		TestEqual(TEXT("Wrong-colour lock"), Xenon->GetVoidTuning().Void.DulledSeconds, 3.0f);
		TestEqual(TEXT("Mixed share at difficulty 1"), Xenon->GetMixedVoidChance(), 0.3f);
	}

	for (const UGolemXenonDefinition* Xenon : { Grunt, Shield })
	{
		TestTrue(TEXT("Every Xenon has at least one attack"), Xenon->GetBehavior().Moves.Num() >= 1);
		for (const FGolemXenonMove& Move : Xenon->GetBehavior().Moves)
		{
			TestTrue(TEXT("Every attack is shown for at least 0.6 s"), Move.TelegraphSeconds >= 0.6f);
		}
	}
	TestTrue(TEXT("The Grunt sweeps wide"), Grunt->GetBehavior().Moves.Num() > 0
		&& Grunt->GetBehavior().Moves[0].HalfAngleDegrees >= 60.0f && Grunt->GetBehavior().Moves[0].Knockback == 0.0f);
	TestTrue(TEXT("The Shield shoves Golems back"), Shield->GetBehavior().Moves.Num() > 0
		&& Shield->GetBehavior().Moves[0].Knockback > 0.0f);
	TestTrue(TEXT("The Shield turns slowly enough to get behind it"),
		Shield->GetBehavior().TurnRateDegrees < Grunt->GetBehavior().TurnRateDegrees);
	for (const UGolemXenonDefinition* Xenon : { Grunt, Shield })
	{
		// A void a hammer cannot reach would lock Mars out of the rule: keep them at Golem height.
		const float FloorToCentre = 90.0f * Xenon->GetBodyScale();
		for (const FGolemXenonVoidSlot& Slot : Xenon->GetVoidSlots())
		{
			const float Height = FloorToCentre + static_cast<float>(Slot.Offset.Z) * Xenon->GetBodyScale();
			TestTrue(TEXT("Void centre sits between knee and head height of a Golem"), Height > 80.0f && Height < 190.0f);
		}
	}

	const TPair<const TCHAR*, FGameplayTag> Heroes[] =
	{
		{ TEXT("Mars"), GolemGameplayTags::Element_Earth }, { TEXT("Jupiter"), GolemGameplayTags::Element_Sky },
		{ TEXT("Neptune"), GolemGameplayTags::Element_Sea }
	};
	// DEV_SPEC section 4: health, power, agility, colour hold, heavy charge seconds, heavy is a projectile.
	struct FExpectedHero
	{
		float MaxHealth;
		float Power;
		float Agility;
		float ColorHold;
		float HeavyCharge;
		bool bProjectile;
	};
	const FExpectedHero Expected[] =
	{
		{ 140.0f, 12.0f, 90.0f, 0.0f, 1.0f, false },
		{ 90.0f, 10.0f, 110.0f, 0.0f, 0.8f, true },
		{ 80.0f, 9.0f, 100.0f, 0.5f, 0.8f, true }
	};
	int32 HeroIndex = 0;
	uint8 PartyMask = 0;
	for (const TPair<const TCHAR*, FGameplayTag>& Hero : Heroes)
	{
		const UGolemHeroDefinition* Definition = Resolve<UGolemHeroDefinition>(*this, TEXT("GolemHero"), Hero.Key);
		if (Definition != nullptr)
		{
			const FExpectedHero& Want = Expected[HeroIndex];
			TestEqual(TEXT("Hero health"), Definition->GetBaseStats().MaxHealth, Want.MaxHealth);
			TestEqual(TEXT("Hero power"), Definition->GetBaseStats().Power, Want.Power);
			TestEqual(TEXT("Hero agility"), Definition->GetBaseStats().Agility, Want.Agility);
			TestEqual(TEXT("Hero colour hold"), Definition->GetBaseStats().ColorHold, Want.ColorHold);
			TestEqual(TEXT("Heavy charge seconds"), Definition->GetMoves().HeavyChargeSeconds, Want.HeavyCharge);
			TestEqual(TEXT("Heavy attack is a projectile"), Definition->GetMoves().bHeavyIsProjectile, Want.bProjectile);
			TestEqual(TEXT("Projectile range is 25 m"), Definition->GetMoves().ProjectileRange, 2500.0f);
		}
		++HeroIndex;
		EGolemElement Element = EGolemElement::Earth;
		if (Definition == nullptr || !GolemElements::FromTag(Definition->GetElementTag(), Element))
		{
			AddError(FString::Printf(TEXT("%s has no element."), Hero.Key));
			return false;
		}
		TestEqual(TEXT("Hero carries its designed element"), Definition->GetElementTag(), Hero.Value);
		TestTrue(TEXT("Hero has a class"), Definition->GetClassTag().IsValid());
		PartyMask |= GolemElements::Mask(Element);
	}
	TestEqual(TEXT("Together the three Golems cover every element"), PartyMask, GolemElements::AllMask);

	const UGolemCombatProfileDefinition* Profile = Resolve<UGolemCombatProfileDefinition>(*this,
		TEXT("GolemCombatProfile"), TEXT("FoundationCombat"));
	if (Profile == nullptr)
	{
		return false;
	}
	int32 ColorAttacks = 0;
	for (const FGolemAttackDefinition& Attack : Profile->GetAttacks())
	{
		const bool bHeavy = Attack.ActionTag == GolemGameplayTags::Ability_Combat_Heavy;
		TestEqual(TEXT("Only the heavy attack places colour"), Attack.bAppliesColor, bHeavy);
		TestEqual(TEXT("Light hits deal 1.0 x Power, the heavy hit 2.5 x Power"), Attack.PowerScale, bHeavy ? 2.5f : 1.0f);
		ColorAttacks += Attack.bAppliesColor ? 1 : 0;
	}
	TestEqual(TEXT("There is exactly one colour-placing attack"), ColorAttacks, 1);
	const FGolemCombatTuning& Combat = Profile->GetTuning();
	TestTrue(TEXT("A dodge ignores damage for 0.35 s"),
		FMath::IsNearlyEqual(Combat.DodgeInvulnerabilityEnd - Combat.DodgeInvulnerabilityStart, 0.35f, 0.001f));
	TestEqual(TEXT("Dodge recovers in 1.5 s at Agility 100"), Combat.DodgeCooldownSeconds, 1.5f);

	const TCHAR* MapName = TEXT("/Game/Golem/Maps/Playables/L_M0_ColorArena");
	TestTrue(TEXT("The colour arena map exists"), FPackageName::DoesPackageExist(MapName));
	UPackage* MapPackage = LoadPackage(nullptr, MapName, LOAD_None);
	UWorld* Map = MapPackage != nullptr ? UWorld::FindWorldInPackage(MapPackage) : nullptr;
	TestNotNull(TEXT("The map holds a world"), Map);
	if (Map != nullptr && Map->GetWorldSettings() != nullptr)
	{
		TestTrue(TEXT("The map runs the colour arena game mode"),
			Map->GetWorldSettings()->DefaultGameMode == AGolemColorArenaGameMode::StaticClass());
	}
	return true;
}

#endif
