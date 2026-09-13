[CmdletBinding()]
param([string]$EngineRoot, [switch]$Build)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context = Get-HansaBuildContext -EngineRoot $EngineRoot
if ($Build) { & (Join-Path $PSScriptRoot 'Build.ps1') -Configuration Development -EngineRoot $context.EngineRoot }
$artifacts = New-HansaArtifactDirectory -Context $context -Operation 'economy-p33-stage'
$arguments = @($context.ProjectFile, '-run=HansaEconomicDefinitionSeed', '-StageEconomyP33',
    '-unattended', '-nop4', '-NullRHI', '-NoSound', '-ddc=NoZenLocalFallback',
    '-ini:Game:[/Script/Engine.AssetManagerSettings]:+PrimaryAssetTypesToScan=(PrimaryAssetType="GameFeatureData",AssetBaseClass="/Script/GameFeatures.GameFeatureData",bHasBlueprintClasses=False,bIsEditorOnly=True,Directories=,SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=NeverCook))',
    '-DisablePlugins=GameFeatures')
$previousSdk = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', 'Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', '1', 'Process')
    Invoke-HansaNativeCommand -FilePath $context.UnrealEditorCommand -Arguments $arguments `
        -LogPath (Join-Path $artifacts 'Stage.log') -FailureMessage 'P33 candidate staging failed.' | Out-Null
} finally { [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', $previousSdk, 'Process') }
$manifest = Join-Path $context.ProjectRoot 'Docs/Development/EconomyP33/catalog_v9_candidate.json'
$data = Get-Content -Raw -LiteralPath $manifest | ConvertFrom-Json
if ($data.status -ne 'candidate-awaiting-balance-approval' -or $data.definitions.Count -ne 72) { throw 'Candidate manifest is incomplete.' }
Write-Output "P33 candidate staged. Catalog hash: $($data.registryHash). Review: $manifest"
