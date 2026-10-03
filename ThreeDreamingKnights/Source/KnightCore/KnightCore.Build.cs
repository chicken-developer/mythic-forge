using UnrealBuildTool;

public class KnightCore : ModuleRules
{
	public KnightCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GameplayTags", "DeveloperSettings"
		});
		PrivateDependencyModuleNames.Add("AssetRegistry");
	}
}
