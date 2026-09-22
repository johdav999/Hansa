[CmdletBinding()]
param([string]$EngineRoot,[switch]$Finalize,[switch]$SplineRivers)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$worldContext = Get-HansaBuildContext -EngineRoot $EngineRoot
$worldArtifacts = New-HansaArtifactDirectory -Context $worldContext -Operation 'hansa-world-map'
$worldPreviousSdk = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
if ($SplineRivers) {
    if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) { throw 'Close Unreal Editor before applying the staged river migration.' }
    & python -m unittest discover -s (Join-Path $PSScriptRoot 'HansaWorld') -p test_rivers.py -v
    if ($LASTEXITCODE -ne 0) { throw 'River source validation failed; map authoring was not started.' }
}
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
    Invoke-HansaNativeCommand -FilePath $worldContext.UnrealEditorCommand -Arguments @(
        $worldContext.ProjectFile, '-run=HansaWorldMap', '-NullRHI', $(if($SplineRivers){'-SplineRivers'}elseif($Finalize){'-Finalize'}else{'-Build'}),
        '-RenderOffscreen',
        '-DisablePlugins=ModelContextProtocol',
        '-unattended', '-nop4', '-nosplash', '-NoSound',
        '-ini:Game:[/Script/Engine.AssetManagerSettings]:+PrimaryAssetTypesToScan=(PrimaryAssetType="GameFeatureData",AssetBaseClass="/Script/GameFeatures.GameFeatureData",bHasBlueprintClasses=False,bIsEditorOnly=True,Directories=,SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=NeverCook))'
    ) -LogPath (Join-Path $worldArtifacts 'Authoring.log') -FailureMessage 'Hansa world map authoring failed.' | Out-Null
} finally { [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$worldPreviousSdk,'Process') }
Write-Output "Staged Hansa world authoring completed: $worldArtifacts"
