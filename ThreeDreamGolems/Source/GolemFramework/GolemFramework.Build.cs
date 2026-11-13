using UnrealBuildTool;

public class GolemFramework : ModuleRules
{
	public GolemFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GolemCore", "AIModule", "NetCore", "EnhancedInput", "InputCore",
			"GameplayTags", "GameplayAbilities", "GolemAbilitySystem", "GolemCombat"
		});
	}
}
