// Copyright Shambler Studio, LLC. All Rights Reserved.


using UnrealBuildTool;

public class ScytheCommon : ModuleRules
{
    public ScytheCommon(ReadOnlyTargetRules Target) : base(Target)
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
                "ScytheDataAssets",
                "ScytheSettings",
                "RenderCore",
                "InteractiveToolsFramework",
                "DynamicMesh",
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
                "EditorFramework",
                "MeshDescription",
                "StaticMeshDescription",
                "PropertyEditor",
                "Projects",
                "EditorInteractiveToolsFramework",
                "GeometryCore",
                "CommonMenuExtensions", 
                "GeometryFramework",
            }
        );
    }
}

