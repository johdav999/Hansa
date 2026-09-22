[CmdletBinding()]
param([string]$EngineRoot,[switch]$Repair,[switch]$RiverOnly)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$worldContext = Get-HansaBuildContext -EngineRoot $EngineRoot
$worldArtifacts = New-HansaArtifactDirectory -Context $worldContext -Operation 'hansa-world-review'
$worldLog = Join-Path $worldArtifacts 'Unreal.log'
$worldPreviousSdk = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
    Invoke-HansaNativeCommand -FilePath $worldContext.UnrealEditorCommand -Arguments @(
        $worldContext.ProjectFile, '-HansaWorldReview', '-unattended', '-nop4', '-nosplash',
        $(if($Repair){'-HansaWorldRepair'}else{'-HansaWorldReadOnlyReview'}),
        $(if($RiverOnly){'-HansaRiverOnly'}else{'-HansaWorldAllViews'}),
        '-NoSound', '-RenderOffscreen', '-windowed', '-ResX=1920', '-ResY=1080',
        '-DisablePlugins=ModelContextProtocol',
        '-ini:Game:[/Script/Engine.AssetManagerSettings]:+PrimaryAssetTypesToScan=(PrimaryAssetType="GameFeatureData",AssetBaseClass="/Script/GameFeatures.GameFeatureData",bHasBlueprintClasses=False,bIsEditorOnly=True,Directories=,SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=NeverCook))',
        '-ExecCmds=Automation RunTests Hansa.World.Campaign.Review',
        '-TestExit=Automation Test Queue Empty', "-AbsLog=$worldLog"
    ) -LogPath (Join-Path $worldArtifacts 'Command.log') -FailureMessage 'Hansa world review failed.' | Out-Null
} finally { [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$worldPreviousSdk,'Process') }
$worldResult = Get-Content -Raw -LiteralPath $worldLog
if ($worldResult -notmatch 'Test Completed. Result=\{Success\} Name=\{Review\}') { throw "World review did not pass: $worldLog" }
Write-Output "World review passed: $worldArtifacts"
