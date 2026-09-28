// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AchievementPlugin : ModuleRules
{
	public AchievementPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"UMG",
				"Slate",
				"SlateCore"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects"
			}
			);
	}
}
