using UnrealBuildTool;

public class GolemOnline : ModuleRules
{
	public GolemOnline(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("GolemCore");
	}
}
