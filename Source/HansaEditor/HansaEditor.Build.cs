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
            "AssetTools",
            "MeshDescription",
            "StaticMeshDescription",
            "RenderCore",
			"Core",
			"CoreUObject",
			"DesktopPlatform",
			"EditorFramework",
			"Engine",
			"Hansa",
			"HansaSimulation",
			"InputCore",
			"Json",
			"JsonUtilities",
			"Landscape",
            "Water",
			"Foliage",
			"GeoReferencing",
			"ToolsetRegistry",
			"LevelEditor",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UnrealEd"
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.AddRange(new string[] { "Advapi32.lib", "Bcrypt.lib" });
		}
	}
}
