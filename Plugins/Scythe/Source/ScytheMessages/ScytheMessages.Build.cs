using UnrealBuildTool;

public class ScytheMessages : ModuleRules
{
	public ScytheMessages(ReadOnlyTargetRules Target) : base(Target)
	{
		bUsePrecompiled=true;

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"EditorSubsystem",
				"Slate",
				"SlateCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"ToolMenus",
				"StatusBar",
				"LevelEditor"
			}
		);
	}
}
