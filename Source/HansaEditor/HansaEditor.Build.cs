using UnrealBuildTool;

public class HansaEditor : ModuleRules
{
	public HansaEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.Type != TargetType.Editor)
		{
			throw new BuildException("HansaEditor may only be built for Editor targets.");
		}

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetRegistry",
			"Core",
			"CoreUObject",
			"EditorFramework",
			"Engine",
			"Hansa",
			"HansaSimulation",
			"InputCore",
			"Json",
			"LevelEditor",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UnrealEd"
		});
	}
}
