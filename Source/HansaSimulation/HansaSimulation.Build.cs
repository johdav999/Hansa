using UnrealBuildTool;

public class HansaSimulation : ModuleRules
{
	public HansaSimulation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDefinitions.Add($"WITH_HANSA_AUTOMATION={(Target.Configuration != UnrealTargetConfiguration.Shipping && Target.bBuildDeveloperTools ? 1 : 0)}");

		PublicDependencyModuleNames.Add("Core");
	}
}
