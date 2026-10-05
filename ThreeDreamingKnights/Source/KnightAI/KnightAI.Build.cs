using UnrealBuildTool;

public class KnightAI : ModuleRules
{
	public KnightAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "KnightCore", "AIModule",
			"StateTreeModule", "GameplayStateTreeModule", "GameplayTags", "KnightAbilitySystem", "GameplayAbilities",
			"KnightFramework"
		});
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"NavigationSystem", "KnightCombat"
		});
	}
}
