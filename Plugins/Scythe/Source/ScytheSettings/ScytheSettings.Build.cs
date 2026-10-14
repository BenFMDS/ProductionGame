// Copyright Shambler Studio, LLC. All Rights Reserved.


using UnrealBuildTool;

public class ScytheSettings : ModuleRules
{
    public ScytheSettings(ReadOnlyTargetRules Target) : base(Target)
    {
        bUsePrecompiled=true;
        
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "DeveloperSettings",
                "PhysicsCore",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "RenderCore", // IsStaticLightingAllowed
                "Slate",
                "SlateCore",
                // "LevelEditor"
                "UMG"
            }
        );
    }
}
