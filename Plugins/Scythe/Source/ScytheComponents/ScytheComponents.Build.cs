// Copyright Shambler Studio, LLC. All Rights Reserved.


using UnrealBuildTool;

using System.IO;

public class ScytheComponents : ModuleRules
{
    public ScytheComponents(ReadOnlyTargetRules Target) : base(Target)
    {
        bUsePrecompiled=true;
        
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "PhysicsCore",
                "InteractiveToolsFramework",
                "MeshDescription",
                "StaticMeshDescription",
                "GeometryCore",
                "GeometryFramework",
                "GeometryAlgorithms",
                "DynamicMesh",
                "MeshConversion",
                "ModelingOperators",
                "DeveloperSettings",
                "InputCore",
                "UnrealEd",
                "ScytheDataAssets",
                "ScytheCommon", // hotspot delegate and selection command scopes
                // "ScytheSettings",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "ModelingComponents",
                "DynamicMesh",
                "RenderCore",
                "ModelingComponentsEditorOnly", // used for converting volume to mesh
                // "EditorFramework",
                "GeometryScriptingCore",
                // "ScytheDataAssets",
                "ScytheSettings",
                "ScytheMessages",
                "EditorSubsystem",
                "Slate",
                "SlateCore",
                // "ScytheCommon"
            }
        );
        
        PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Plugins/Runtime/GeometryProcessing/Source/GeometryAlgorithms/Private"));
    }
}

