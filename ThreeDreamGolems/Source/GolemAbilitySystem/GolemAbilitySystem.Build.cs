using UnrealBuildTool;

public class GolemAbilitySystem : ModuleRules
{
	public GolemAbilitySystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GolemCore", "GameplayAbilities", "GameplayTags", "GameplayTasks"
		});
	}
}
