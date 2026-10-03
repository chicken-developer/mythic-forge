// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ThreeDreamingKnights : ModuleRules
{
	public ThreeDreamingKnights(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "KnightCore", "KnightFramework", "KnightAbilitySystem",
			"GameplayAbilities", "KnightCombat"
		});

		// The existing primary module owns the KnightGame composition role.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AIModule",
			"GameplayTags",
			"KnightAI",
			"KnightItems",
			"KnightQuest",
			"KnightDialogue",
			"KnightWorld",
			"KnightUI",
			"KnightOnline",
			"KnightSave"
		});
	}
}
