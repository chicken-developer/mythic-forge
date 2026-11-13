using UnrealBuildTool;

public class GolemItems : ModuleRules
{
	public GolemItems(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "NetCore", "GolemCore", "GolemAbilitySystem", "GameplayTags"
		});
		PrivateDependencyModuleNames.AddRange(new string[] { "GameplayAbilities" });
	}
}
