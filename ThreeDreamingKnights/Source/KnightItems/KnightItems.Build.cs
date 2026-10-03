using UnrealBuildTool;

public class KnightItems : ModuleRules
{
	public KnightItems(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("KnightCore");
	}
}
