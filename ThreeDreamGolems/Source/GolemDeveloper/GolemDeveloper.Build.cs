using UnrealBuildTool;

public class GolemDeveloper : ModuleRules
{
	public GolemDeveloper(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ThreeDreamGolems", "GolemCore", "GolemFramework", "GolemAI", "UnrealEd", "AssetRegistry",
			"StateTreeModule", "StateTreeEditorModule", "GameplayStateTreeModule", "NavigationSystem",
			"PropertyBindingUtils", "AIModule", "EnhancedInput", "InputCore", "GameplayTags", "Sockets", "ApplicationCore",
			"GolemAbilitySystem", "GameplayAbilities", "GameplayTasks", "GolemCombat",
			"AnimationDataController", "AnimGraphRuntime", "AssetTools", "LevelSequence", "AIGraph",
			"EnvironmentQueryEditor", "GolemItems", "GolemUI", "UMG", "CommonUI", "Slate", "SlateCore", "Json",
			"RenderCore", "RHI", "GolemQuest", "GolemDialogue", "GolemWorld", "GolemSave"
		});
	}
}
