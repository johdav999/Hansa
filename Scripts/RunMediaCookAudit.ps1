[CmdletBinding()]
param([string]$EngineRoot, [string]$ArtifactsRoot)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context = Get-HansaBuildContext -EngineRoot $EngineRoot -ArtifactsRoot $ArtifactsRoot
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation 'media-shipping-cook'
$cookedRoot = Join-Path $artifactDirectory 'Cooked'
$disabledPlugins = 'ModelContextProtocol,AllToolsets,GameFeatures,Water,Landmass'
$previousSdk = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', 'Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', '1', 'Process')
    Invoke-HansaNativeCommand -FilePath $context.UnrealEditorCommand -Arguments @(
        $context.ProjectFile, '-run=Cook', '-TargetPlatform=Windows',
        '-Map=/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP', '-ClientConfig=Shipping',
        '-unattended', '-nop4', '-NullRHI', '-SkipEditorContent',
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
    Scope = 'Lubeck cooked content and media boundaries; process-only authoring-plugin exclusions'
    DisabledPlugins = $disabledPlugins.Split(',')
    ProjectConfigurationModified = $false
    CompletedUtc = [DateTime]::UtcNow.ToString('o')
})
Write-Output "Media cook evidence: $artifactDirectory"
