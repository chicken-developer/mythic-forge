using UnrealBuildTool;

public class DreamBPGen : ModuleRules
{
	public DreamBPGen(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "BlueprintGraph", "Kismet", "KismetCompiler", "AssetRegistry" });
		}
	}
}
