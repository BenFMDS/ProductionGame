using UnrealBuildTool;

public class HotspotEditor : ModuleRules
{
    public HotspotEditor(ReadOnlyTargetRules Target) : base(Target)
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
                "ModelingComponentsEditorOnly", // Static/skeletal mesh tool targets
                "ModelingComponents",
                "StatusBar",
                "ToolMenus",
                "ToolWidgets",
                "WorkspaceMenuStructure",
                "TextureUtilitiesCommon",
                "GeometryAlgorithms",
                "PropertyEditor", 
                "ScytheComponents", // using some grid stuff in the rect tool. Could probably skip this.
                "ScytheCommon", // to get texture sizes
                "ScytheSettings",
                "ScytheMessages",
                "ScytheDataAssets", 
                "Projects",
                "AssetRegistry"
            }
        );
    }
}

