using UnrealBuildTool;

public class GolemCombat : ModuleRules
{
	public GolemCombat(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GolemCore", "GolemAbilitySystem", "GameplayAbilities",
			"GameplayTags", "GameplayTasks"
		});
		PrivateDependencyModuleNames.AddRange(new string[] { "AnimGraphRuntime", "NavigationSystem", "AIModule" });
	}
}
