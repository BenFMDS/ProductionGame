// Copyright Shambler Studio, LLC. All Rights Reserved.


using UnrealBuildTool;

public class ScytheDataAssets : ModuleRules
{
    public ScytheDataAssets(ReadOnlyTargetRules Target) : base(Target)
    {
        bUsePrecompiled=true;
        
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "AssetTools",
                "UnrealEd",
                "AssetDefinition",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "InputCore",
                "AdvancedPreviewScene",
                "InteractiveToolsFramework",
                "ContentBrowser",
                "DynamicMesh",
                "EditorFramework", // FEditorModeInfo
                "EditorInteractiveToolsFramework",			
                "EditorSubsystem",
                "GeometryCore",
                "LevelEditor",
                "StatusBar",
                "ToolMenus",
                "ToolWidgets",
                "WorkspaceMenuStructure",
                "TextureUtilitiesCommon",
                "GeometryAlgorithms",
                "PropertyEditor", 
            }
        );
    }
}
