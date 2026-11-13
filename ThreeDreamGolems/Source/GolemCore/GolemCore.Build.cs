using UnrealBuildTool;

public class GolemCore : ModuleRules
{
	public GolemCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GameplayTags", "DeveloperSettings"
		});
		PrivateDependencyModuleNames.Add("AssetRegistry");
	}
}
