using UnrealBuildTool;

public class KnightDeveloper : ModuleRules
{
	public KnightDeveloper(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ThreeDreamingKnights", "KnightCore", "KnightFramework", "KnightAI", "UnrealEd", "AssetRegistry",
			"StateTreeModule", "StateTreeEditorModule", "GameplayStateTreeModule", "NavigationSystem",
			"PropertyBindingUtils", "AIModule", "EnhancedInput", "InputCore", "GameplayTags", "Sockets",
			"KnightAbilitySystem", "GameplayAbilities", "GameplayTasks", "KnightCombat", "MotionWarping",
			"AnimationDataController", "AnimGraphRuntime", "AssetTools", "LevelSequence", "AIGraph",
			"EnvironmentQueryEditor", "KnightItems", "KnightUI", "UMG", "CommonUI", "Slate", "SlateCore"
		});
	}
}
