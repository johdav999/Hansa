[CmdletBinding()]
param([string]$EngineRoot, [string]$ArtifactsRoot)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context = Get-HansaBuildContext -EngineRoot $EngineRoot -ArtifactsRoot $ArtifactsRoot
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation 'media-shipping-cook'
$cookedRoot = Join-Path $artifactDirectory 'Cooked'
$disabledPlugins = 'GameFeatures'
# HansaEditor links ToolsetRegistry and Water for native authoring. Keep those editor dependencies loadable.
# TargetAllowList and the cooked/receipt audit enforce Shipping exclusion; disabling their plugins here prevents the commandlet from starting.
$previousSdk = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', 'Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', '1', 'Process')
    Invoke-HansaNativeCommand -FilePath $context.UnrealEditorCommand -Arguments @(
        $context.ProjectFile, '-run=Cook', '-TargetPlatform=Windows',
        '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
        '-ini:Game:[/Script/Engine.AssetManagerSettings]:+PrimaryAssetTypesToScan=(PrimaryAssetType="GameFeatureData",AssetBaseClass="/Script/GameFeatures.GameFeatureData",bHasBlueprintClasses=False,bIsEditorOnly=True,Directories=,SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=NeverCook))',
        '-Map=/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP', '-ClientConfig=Shipping',
        '-unattended', '-nop4', '-NullRHI', '-SkipEditorContent', '-SkipZenStore',
        '-ini:Game:[/Script/UnrealEd.ProjectPackagingSettings]:+DirectoriesToNeverCook=(Path="/Landmass/Landscape/BlueprintBrushes")',
        "-DisablePlugins=$disabledPlugins", "-OutputDir=$cookedRoot"
    ) -LogPath (Join-Path $artifactDirectory 'Cook.log') -FailureMessage 'Media Shipping cook failed' | Out-Null
} catch {
    Write-HansaJsonArtifact -Path (Join-Path $artifactDirectory 'result.json') -Value ([ordered]@{
        Operation = 'RunMediaCookAudit'; Status = 'Failed'; FailureStage = 'Cook'
        CookedRoot = $cookedRoot; DisabledPlugins = $disabledPlugins.Split(',')
        ProjectConfigurationModified = $false
        CookLog = Join-Path $artifactDirectory 'Cook.log'
        CompletedUtc = [DateTime]::UtcNow.ToString('o')
    })
    throw
} finally {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', $previousSdk, 'Process')
}
& (Join-Path $PSScriptRoot 'VerifyMediaShipping.ps1') -EngineRoot $context.EngineRoot -ArtifactsRoot $context.ArtifactsRoot -CookedRoot $cookedRoot -RequireCookedContent
Write-HansaJsonArtifact -Path (Join-Path $artifactDirectory 'result.json') -Value ([ordered]@{
    Operation = 'RunMediaCookAudit'; Status = 'Succeeded'; CookedRoot = $cookedRoot
    Scope = 'Lubeck cooked content and media boundaries; required editor dependencies enabled'
    DisabledPlugins = $disabledPlugins.Split(',')
    ProjectConfigurationModified = $false
    CompletedUtc = [DateTime]::UtcNow.ToString('o')
})
Write-Output "Media cook evidence: $artifactDirectory"
