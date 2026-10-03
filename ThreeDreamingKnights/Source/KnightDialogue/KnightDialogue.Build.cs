using UnrealBuildTool;

public class KnightDialogue : ModuleRules
{
	public KnightDialogue(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("KnightCore");
	}
}
