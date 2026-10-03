using UnrealBuildTool;

public class KnightOnline : ModuleRules
{
	public KnightOnline(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("KnightCore");
	}
}
