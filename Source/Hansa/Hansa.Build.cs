using UnrealBuildTool;

public class Hansa : ModuleRules
{
	public Hansa(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		RuntimeDependencies.Add("$(ProjectDir)/Content/Hansa/UI/Icons/*.png", StagedFileType.UFS);
        RuntimeDependencies.Add("$(ProjectDir)/Content/Hansa/UI/ArtisanProduction/*.png", StagedFileType.UFS);
        RuntimeDependencies.Add("$(ProjectDir)/Content/Hansa/UI/ReferenceHud/*.png", StagedFileType.UFS);
        RuntimeDependencies.Add("$(ProjectDir)/Content/Hansa/UI/Production/*.png", StagedFileType.UFS);
        RuntimeDependencies.Add("$(ProjectDir)/Content/Hansa/UI/Residence/*.png", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Content/Hansa/UI/TextileProduction/*.png", StagedFileType.UFS);
		// Slate reads these through the platform file API in packaged games.
		RuntimeDependencies.Add("$(ProjectDir)/Content/Hansa/UI/Fonts/*.ttf", StagedFileType.UFS);
		RuntimeDependencies.Add("$(ProjectDir)/Content/Hansa/UI/Fonts/*OFL*", StagedFileType.UFS);
		PublicDefinitions.Add($"WITH_HANSA_AUTOMATION={(Target.Configuration != UnrealTargetConfiguration.Shipping && Target.bBuildDeveloperTools ? 1 : 0)}");

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
        {
            PrivateDependencyModuleNames.Add("AssetRegistry");

        }

        PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HansaSimulation",
			"SlateCore",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Landscape",
            "Water",
			"ProceduralMeshComponent",
            "RenderCore",
            "RHI",
			"EnhancedInput",
			"InputCore"
		});
	}
}
