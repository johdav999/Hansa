using UnrealBuildTool;
using System.Collections.Generic;

public class HansaTarget : TargetRules
{
	public HansaTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

		bool bWithHansaAutomation = Configuration != UnrealTargetConfiguration.Shipping;
		bBuildDeveloperTools = bWithHansaAutomation;

		ExtraModuleNames.Add("Hansa");
		if (bWithHansaAutomation)
		{
			ExtraModuleNames.Add("HansaAutomation");
		}
	}
}
