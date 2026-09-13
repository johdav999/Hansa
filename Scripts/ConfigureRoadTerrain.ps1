[CmdletBinding()]
param([string]$EngineRoot)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context=Get-HansaBuildContext -EngineRoot $EngineRoot
$artifacts=New-HansaArtifactDirectory -Context $context -Operation 'road-terrain-materials'
$previousSdk=[Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
    Invoke-HansaNativeCommand -FilePath $context.UnrealEditorCommand -Arguments @(
        $context.ProjectFile, '-run=HansaRoadMaterial', '-unattended', '-nop4', '-nosplash', '-NullRHI',
        '-ini:Game:[/Script/Engine.AssetManagerSettings]:+PrimaryAssetTypesToScan=(PrimaryAssetType="GameFeatureData",AssetBaseClass="/Script/GameFeatures.GameFeatureData",bHasBlueprintClasses=False,bIsEditorOnly=True,Directories=,SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=NeverCook))'
    ) -LogPath (Join-Path $artifacts 'Authoring.log') -FailureMessage 'Road terrain material authoring failed.' | Out-Null
} finally { [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$previousSdk,'Process') }
Write-Output "Road terrain materials configured. Evidence: $artifacts"
