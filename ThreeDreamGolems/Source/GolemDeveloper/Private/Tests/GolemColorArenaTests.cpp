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

	const TPair<const TCHAR*, FGameplayTag> Heroes[] =
	{
		{ TEXT("Mars"), GolemGameplayTags::Element_Earth }, { TEXT("Jupiter"), GolemGameplayTags::Element_Sky },
		{ TEXT("Neptune"), GolemGameplayTags::Element_Sea }
	};
	uint8 PartyMask = 0;
	for (const TPair<const TCHAR*, FGameplayTag>& Hero : Heroes)
	{
		const UGolemHeroDefinition* Definition = Resolve<UGolemHeroDefinition>(*this, TEXT("GolemHero"), Hero.Key);
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
		ColorAttacks += Attack.bAppliesColor ? 1 : 0;
	}
	TestEqual(TEXT("There is exactly one colour-placing attack"), ColorAttacks, 1);

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
