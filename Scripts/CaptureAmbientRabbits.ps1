[CmdletBinding()]
param([string]$EngineRoot = 'H:\Unreal\UE_5.8')
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context = Get-HansaBuildContext -EngineRoot $EngineRoot
$artifacts = New-HansaArtifactDirectory -Context $context -Operation 'rabbit-gameplay-capture'
$unrealLog = Join-Path $artifacts 'Unreal.log'
$previousSdk = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
    Invoke-HansaNativeCommand -FilePath $context.UnrealEditorCommand -Arguments @(
        $context.ProjectFile,'/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/L_Lubeck_Terrain_Preview_WP',
        '-game','-unattended','-nop4','-nosplash','-NoSound','-windowed','-ForceRes','-ResX=1920','-ResY=1080','-RenderOffscreen',
        '-ddc=NoZenLocalFallback','-ExecCmds=Automation RunTests Hansa.World.Rabbits.GameplayCapture',
        '-TestExit=Automation Test Queue Empty',"-AbsLog=$unrealLog"
    ) -LogPath (Join-Path $artifacts 'Command.log') -FailureMessage 'Rabbit capture process failed.' | Out-Null
} finally { [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$previousSdk,'Process') }
$log = Get-Content -Raw -LiteralPath $unrealLog
if ($log -notmatch 'Test Completed. Result=\{Success\} Name=\{GameplayCapture\}' -or $log -match 'Result=\{Fail|Failed to compile Material|Expression is part of a cycle') {
    throw "Rabbit gameplay capture failed: $unrealLog"
}
Write-Output "Rabbit gameplay capture passed: $artifacts"
