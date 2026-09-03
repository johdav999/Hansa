using UnrealBuildTool;

public class HansaAutomation : ModuleRules
{
	public HansaAutomation(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
			throw new BuildException("HansaAutomation must not be built for Shipping targets.");
		}

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDefinitions.Add("WITH_HANSA_AUTOMATION=1");

		PublicDependencyModuleNames.Add("Core");

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"Hansa",
			"HansaSimulation",
			"Json",
			"Slate",
			"SlateCore"
		});
	}
}
