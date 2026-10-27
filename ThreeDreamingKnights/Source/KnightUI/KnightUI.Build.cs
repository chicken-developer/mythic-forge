using UnrealBuildTool;

public class KnightUI : ModuleRules
{
	public KnightUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "KnightCore", "KnightAbilitySystem", "CommonUI", "UMG", "InputCore"
		});
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "CommonInput" });
	}
}
