// Copyright Shambler Studio, LLC. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Scythe : ModuleRules
{
	public Scythe(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled=true;

		PublicIncludePaths.AddRange(
			new string[]
			{
			}
		);


		PrivateIncludePaths.AddRange(
			new string[]
			{
			}
		);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GeometryCore"
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"ApplicationCore",
				"AssetRegistry",
				"Slate",
				"SlateCore",
				"AdvancedPreviewScene",
				"Engine",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"ContentBrowser",
				"ContentBrowserData",
				"Projects",
				"TypedElementRuntime",
				"InteractiveToolsFramework",
				"LevelEditor",
				"EditorInteractiveToolsFramework",
				"GeometryFramework",
				"GeometryCore",
				"GeometryProcessingInterfaces",
				"DynamicMesh",
				"ModelingComponents",
				"ModelingComponentsEditorOnly",
				"MeshModelingTools",
				"MeshModelingToolsExp",
				"MeshModelingToolsEditorOnly",
				"MeshModelingToolsEditorOnlyExp",
				"MeshLODToolset",
				"ToolWidgets",
				"EditorWidgets",
				"ModelingEditorUI",
				"WidgetRegistration",
				"DeveloperSettings",
				"PropertyEditor",
				"ToolMenus",
				"EditorConfig",
				"ToolPresetAsset",
				"ToolPresetEditor",
				"EditorConfig",
				"ModelingToolsEditorMode",
				"ScytheTools",
				"ScytheComponents",
				"GeometryScriptingCore",
				"ModelingOperators",
				"ScytheSettings",
				"ScytheCommon",
				"ScytheMessages",
				"RenderCore",
				"ScytheMaterialBrowser",
				"UMG",
				"EditorSubsystem"
			}
		);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);

		bool bSupportsViewportInteractions = Target.Version.MajorVersion > 5  || (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 8);
		if (bSupportsViewportInteractions)
		{
			PrivateIncludePathModuleNames.Add("ScytheViewportInteractions");
			DynamicallyLoadedModuleNames.Add("ScytheViewportInteractions");
		}
	}
}

