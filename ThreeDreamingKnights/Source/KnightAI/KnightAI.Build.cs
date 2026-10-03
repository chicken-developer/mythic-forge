using UnrealBuildTool;

public class KnightAI : ModuleRules
{
	public KnightAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "KnightCore", "KnightFramework", "AIModule",
			"StateTreeModule", "GameplayStateTreeModule"
		});
		PrivateDependencyModuleNames.Add("NavigationSystem");
	}
}
