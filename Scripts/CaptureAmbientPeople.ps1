[CmdletBinding()]
param([ValidateSet(1280,1920)][int]$Width=1920,[ValidateSet(720,1080)][int]$Height=1080)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context=Get-HansaBuildContext
$artifacts=New-HansaArtifactDirectory -Context $context -Operation "people-gameplay-$Width-$Height"
$unrealLog=Join-Path $artifacts 'Unreal.log'
$previousSdk=[Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
    Invoke-HansaNativeCommand -FilePath $context.UnrealEditorCommand -Arguments @(
        $context.ProjectFile,'/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/L_Lubeck_Terrain_Preview_WP',
        '-game','-unattended','-nop4','-nosplash','-NoSound','-windowed','-ForceRes',"-ResX=$Width","-ResY=$Height",'-RenderOffscreen',
        '-DisablePlugins=ModelContextProtocol','-ddc=NoZenLocalFallback',
        '-ExecCmds=Automation RunTests Hansa.World.People.GameplayCapture','-TestExit=Automation Test Queue Empty',"-AbsLog=$unrealLog"
    ) -LogPath (Join-Path $artifacts 'Command.log') -FailureMessage 'People viewport capture failed.' | Out-Null
} finally { [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$previousSdk,'Process') }
$log=Get-Content -Raw -LiteralPath $unrealLog
if($log -notmatch 'Test Completed. Result=\{Success\} Name=\{GameplayCapture\}' -or $log -match 'Result=\{Fail|Failed to compile Material|M_Laborer01.*missing usage') {
    throw "People gameplay capture failed: $unrealLog"
}
Write-Output "People gameplay capture passed: $artifacts"
