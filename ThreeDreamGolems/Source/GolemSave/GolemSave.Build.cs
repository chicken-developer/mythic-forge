using UnrealBuildTool;

public class GolemSave : ModuleRules
{
	public GolemSave(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GolemCore", "GolemItems", "GolemQuest", "GolemDialogue", "GolemWorld",
			"GolemAbilitySystem", "GameplayTags"
		});
	}
}
