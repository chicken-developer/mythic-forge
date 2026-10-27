using UnrealBuildTool;

public class KnightWorld : ModuleRules
{
	public KnightWorld(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "KnightCore" });
	}
}
