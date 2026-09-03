using UnrealBuildTool;

public class Hansa : ModuleRules
{
	public Hansa(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDefinitions.Add($"WITH_HANSA_AUTOMATION={(Target.Configuration != UnrealTargetConfiguration.Shipping && Target.bBuildDeveloperTools ? 1 : 0)}");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HansaSimulation"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EnhancedInput",
			"InputCore"
		});
	}
}
