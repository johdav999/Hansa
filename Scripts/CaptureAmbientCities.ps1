[CmdletBinding()]
param(
    [ValidateSet(1280,1920)][int]$Width=1920,
    [ValidateSet(720,1080)][int]$Height=1080,
    [ValidateSet('Development','DebugGame')][string]$Configuration='Development'
)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context=Get-HansaBuildContext
$artifacts=New-HansaArtifactDirectory -Context $context -Operation "ambient-cities-$Width-$Height"
$unrealLog=Join-Path $artifacts 'Unreal.log'
$editorCommand=$context.UnrealEditorCommand
if($Configuration -eq 'DebugGame') { $editorCommand=Join-Path (Split-Path $editorCommand) 'UnrealEditor-Win64-DebugGame-Cmd.exe' }
$previousSdk=[Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
    Invoke-HansaNativeCommand -FilePath $editorCommand -Arguments @(
        $context.ProjectFile,'/Game/Hansa/World/HansaWorld_20260918/L_HansaWorld_WP',
        '-game','-unattended','-nop4','-nosplash','-NoSound','-windowed','-ForceRes',"-ResX=$Width","-ResY=$Height",'-RenderOffscreen',
        '-DisablePlugins=ModelContextProtocol','-ddc=NoZenLocalFallback',
        '-ExecCmds=Automation RunTests Hansa.World.AmbientCities.GameplayCapture','-TestExit=Automation Test Queue Empty',"-AbsLog=$unrealLog"
    ) -LogPath (Join-Path $artifacts 'Command.log') -FailureMessage 'Ambient city viewport capture failed.' | Out-Null
} finally { [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$previousSdk,'Process') }
$log=Get-Content -Raw -LiteralPath $unrealLog
if($log -notmatch 'Test Completed. Result=\{Success\} Name=\{GameplayCapture\}' -or $log -match 'Result=\{Fail') {
    throw "Ambient city capture failed: $unrealLog"
}
Write-Output "Ambient city capture passed: $artifacts"
