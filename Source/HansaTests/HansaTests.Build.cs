using UnrealBuildTool;

public class HansaTests : ModuleRules
{
	// Keep new multiplayer command-path and projection sources visible to UBT's generated makefile.
	// Coverage includes deterministic simulation, surveyed waterfront construction,
	// versioned fixtures, and managed world-projection lifecycles.
	public HansaTests(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
			throw new BuildException("HansaTests must not be built for Shipping targets.");
		}

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Hansa",
			"HansaAutomation",
			"HansaSimulation",
			"InputCore",
			"Json",
			"Landscape",
			"ProceduralMeshComponent",
            "RenderCore",
            "RHI",
			"Slate",
			"SlateCore"
		});
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add("MeshDescription");
			PrivateDependencyModuleNames.Add("StaticMeshDescription");
		}
	}
}
