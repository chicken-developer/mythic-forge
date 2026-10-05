using UnrealBuildTool;

public class KnightItems : ModuleRules
{
	public KnightItems(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "NetCore", "KnightCore", "KnightAbilitySystem", "GameplayTags"
		});
		PrivateDependencyModuleNames.AddRange(new string[] { "GameplayAbilities" });
	}
}
