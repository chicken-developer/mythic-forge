using UnrealBuildTool;

public class KnightCombat : ModuleRules
{
	public KnightCombat(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "KnightCore", "KnightAbilitySystem", "GameplayAbilities",
			"GameplayTags", "GameplayTasks", "MotionWarping"
		});
		PrivateDependencyModuleNames.AddRange(new string[] { "AnimGraphRuntime", "NavigationSystem", "AIModule" });
	}
}
