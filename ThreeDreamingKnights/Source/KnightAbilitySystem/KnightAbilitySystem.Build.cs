using UnrealBuildTool;

public class KnightAbilitySystem : ModuleRules
{
	public KnightAbilitySystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "KnightCore", "GameplayAbilities", "GameplayTags", "GameplayTasks"
		});
	}
}
