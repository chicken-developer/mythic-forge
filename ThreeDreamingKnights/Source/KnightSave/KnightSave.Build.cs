using UnrealBuildTool;

public class KnightSave : ModuleRules
{
	public KnightSave(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("KnightCore");
	}
}
