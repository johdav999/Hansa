using UnrealBuildTool;

public class HansaTests : ModuleRules
{
	// Coverage includes deterministic simulation, versioned fixtures, and managed world-projection lifecycles.
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
			"Json"
		});
	}
}
