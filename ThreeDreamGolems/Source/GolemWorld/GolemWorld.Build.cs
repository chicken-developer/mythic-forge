using UnrealBuildTool;

public class GolemWorld : ModuleRules
{
	public GolemWorld(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GolemCore" });
	}
}
