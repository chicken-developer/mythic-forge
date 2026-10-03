using UnrealBuildTool;

public class KnightUI : ModuleRules
{
	public KnightUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("KnightCore");
	}
}
