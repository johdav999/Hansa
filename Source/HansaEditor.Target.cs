using UnrealBuildTool;
using System.Collections.Generic;

public class HansaEditorTarget : TargetRules
{
	public HansaEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		bBuildDeveloperTools = true;

		ExtraModuleNames.AddRange(new string[]
		{
			"Hansa",
			"HansaEditor",
			"HansaAutomation",
			"HansaTests"
		});
	}
}
