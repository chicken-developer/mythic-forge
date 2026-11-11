// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ThreeDreamGolems : ModuleRules
{
	public ThreeDreamGolems(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GolemCore", "GolemFramework", "GolemAbilitySystem",
			"GameplayAbilities", "GolemCombat", "GolemAI", "GolemItems",
			"GolemQuest", "GolemDialogue", "GolemWorld", "GolemSave", "GolemUI"
		});

		// The existing primary module owns the GolemGame composition role.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AIModule",
			"ApplicationCore",
			"GameplayTags",
			"EnhancedInput", "InputCore", "CommonUI", "CommonInput", "UMG", "SlateCore",
			"GolemOnline"
		});
	}
}
