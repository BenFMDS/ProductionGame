// Copyright Shambler Studio, LLC. All Rights Reserved.


using UnrealBuildTool;

public class ScytheMaterialBrowser : ModuleRules
{
	public ScytheMaterialBrowser(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled=true;

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"EditorSubsystem",
				"ScytheCommon"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"ScytheSettings",
				"ScytheCommon",
				"ScytheMessages",
				"RenderCore",
				"RHI",
				"ContentBrowser",
				"ToolWidgets",
				"ToolMenus",
				"HotspotEditor",
				"AssetTools",
				"AssetRegistry",
				"ScytheDataAssets",
				"Projects",
				"ToolWidgets"
			}
		);
	}
}
