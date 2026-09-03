using UnrealBuildTool;

public class HansaSimulation : ModuleRules
{
	// Runtime-only deterministic simulation systems, including population and local markets.
	public HansaSimulation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDefinitions.Add($"WITH_HANSA_AUTOMATION={(Target.Configuration != UnrealTargetConfiguration.Shipping && Target.bBuildDeveloperTools ? 1 : 0)}");

		PublicDependencyModuleNames.Add("Core");
	}
}
