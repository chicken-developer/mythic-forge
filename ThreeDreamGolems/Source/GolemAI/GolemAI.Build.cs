using UnrealBuildTool;

public class GolemAI : ModuleRules
{
	public GolemAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GolemCore", "AIModule",
			"StateTreeModule", "GameplayStateTreeModule", "GameplayTags", "GolemAbilitySystem", "GameplayAbilities",
			"GolemFramework"
		});
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"NavigationSystem", "GolemCombat"
		});
	}
}
