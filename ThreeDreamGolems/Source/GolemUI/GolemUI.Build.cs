using UnrealBuildTool;

public class GolemUI : ModuleRules
{
	public GolemUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GolemCore", "GolemAbilitySystem", "CommonUI", "UMG", "InputCore"
		});
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "CommonInput" });
	}
}
