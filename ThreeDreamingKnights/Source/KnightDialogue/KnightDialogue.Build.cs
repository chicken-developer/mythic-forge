using UnrealBuildTool;

public class KnightDialogue : ModuleRules
{
	public KnightDialogue(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "KnightCore" });
	}
}
