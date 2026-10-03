using UnrealBuildTool;

public class KnightFramework : ModuleRules
{
	public KnightFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "KnightCore", "AIModule", "NetCore", "EnhancedInput", "InputCore",
			"GameplayTags", "GameplayAbilities", "KnightAbilitySystem", "KnightCombat"
		});
		PrivateDependencyModuleNames.Add("MotionWarping");
	}
}
