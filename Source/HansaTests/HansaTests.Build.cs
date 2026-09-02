using UnrealBuildTool;

public class HansaTests : ModuleRules
{
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
			"Hansa",
			"HansaAutomation",
			"HansaSimulation",
			"Json"
		});
	}
}
