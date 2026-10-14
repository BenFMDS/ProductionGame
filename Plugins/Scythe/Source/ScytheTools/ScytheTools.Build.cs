// Copyright Shambler Studio, LLC. All Rights Reserved.


using UnrealBuildTool;

using System.IO;

public class ScytheTools : ModuleRules
{
    public ScytheTools(ReadOnlyTargetRules Target) : base(Target)
    {
        bUsePrecompiled=true;
        
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "ModelingOperators",
                "GeometryAlgorithms",
                "ScytheCommon", 
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Slate",
                "SlateCore",
                "Engine",
                "InputCore",
                "EditorFramework",
                "UnrealEd",
                "ContentBrowserData",
                "Projects",
                "TypedElementRuntime",
                "InteractiveToolsFramework",
                "EditorInteractiveToolsFramework",
                "GeometryFramework",
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
                "GeometryCore",
                "GeometryFramework",
                "GeometryScriptingCore", 
                "ScytheComponents", 
                "UVEditorTools", // remove this if getting compiling errors
                "StaticMeshDescription",
                "MeshDescription",
                "MeshConversion",
                "ScytheSettings", 
                "ScytheDataAssets",
                "ScytheMessages",
                // "ScytheDataAssets",
                "ScytheMaterialBrowser",
                "EditorSubsystem"
            }
        );
    }
}

