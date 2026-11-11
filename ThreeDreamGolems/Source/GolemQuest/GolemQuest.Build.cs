using UnrealBuildTool;

public class GolemQuest : ModuleRules
{
	public GolemQuest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GolemCore" });
	}
}
