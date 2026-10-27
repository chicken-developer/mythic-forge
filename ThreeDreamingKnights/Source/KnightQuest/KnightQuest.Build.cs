using UnrealBuildTool;

public class KnightQuest : ModuleRules
{
	public KnightQuest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "KnightCore" });
	}
}
