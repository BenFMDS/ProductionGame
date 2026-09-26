using UnrealBuildTool;

public class ProductionGameTools : ModuleRules
{
    public ProductionGameTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "ProductionGame", "AIModule" });
        PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "BlueprintGraph", "Kismet", "KismetCompiler", "AssetRegistry", "UMGEditor", "UMG", "Slate", "SlateCore", "InputCore" });
    }
}
