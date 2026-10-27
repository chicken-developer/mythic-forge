using UnrealBuildTool;

public class KnightSave : ModuleRules
{
	public KnightSave(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "KnightCore", "KnightItems", "KnightQuest", "KnightDialogue", "KnightWorld",
			"KnightAbilitySystem", "GameplayTags"
		});
	}
}
