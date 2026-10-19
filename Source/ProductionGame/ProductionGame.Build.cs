// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProductionGame : ModuleRules
{
	public ProductionGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"AchievementPlugin",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ProductionGame",
			"ProductionGame/Variant_Horror",
			"ProductionGame/Variant_Horror/UI",
			"ProductionGame/Variant_Shooter",
			"ProductionGame/Variant_Shooter/AI",
			"ProductionGame/Variant_Shooter/UI",
			"ProductionGame/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
