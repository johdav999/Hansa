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
			"Core",
			"CoreUObject",
			"Engine",
			"Hansa",
			"HansaSimulation",
			"UnrealEd"
		});
	}
}
