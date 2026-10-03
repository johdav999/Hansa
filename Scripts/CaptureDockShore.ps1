[CmdletBinding()]
param([ValidateSet(1280,1920)][int]$Width=1920,[ValidateSet(720,1080)][int]$Height=1080,[string]$EngineRoot)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context = Get-HansaBuildContext -EngineRoot $EngineRoot
$artifacts = New-HansaArtifactDirectory -Context $context -Operation "dock-shore-viewport-$Width-$Height"
$unrealLog = Join-Path $artifacts 'Unreal.log'
New-Item -ItemType Directory -Force -Path (Join-Path $context.ProjectRoot 'Docs/Images/World/DockPlacement') | Out-Null
$previousSdk = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
    Invoke-HansaNativeCommand -FilePath $context.UnrealEditorCommand -Arguments @(
        $context.ProjectFile,'/Game/Hansa/World/HansaWorld_20260918/L_HansaWorld_WP',
        '-game','-unattended','-nop4','-nosplash','-NoSound','-windowed','-ForceRes',
        "-ResX=$Width","-ResY=$Height",'-RenderOffscreen','-DisablePlugins=ModelContextProtocol',
        '-ExecCmds=Automation RunTests Hansa.World.Harbor.ShoreViewport','-TestExit=Automation Test Queue Empty',"-AbsLog=$unrealLog"
    ) -LogPath (Join-Path $artifacts 'Command.log') -FailureMessage 'Dock shoreline capture failed.' | Out-Null
} finally { [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$previousSdk,'Process') }
$log = Get-Content -Raw -LiteralPath $unrealLog
if ($log -notmatch 'Test Completed. Result=\{Success\} Name=\{ShoreViewport\}' -or $log -match 'Result=\{Fail') {
    throw "Dock shoreline rendered regression failed: $unrealLog"
}
Write-Output "Dock shoreline capture passed: $artifacts"
