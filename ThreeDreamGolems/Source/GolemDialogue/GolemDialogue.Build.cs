using UnrealBuildTool;

public class GolemDialogue : ModuleRules
{
	public GolemDialogue(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GolemCore" });
	}
}
