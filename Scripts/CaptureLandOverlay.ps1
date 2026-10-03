[CmdletBinding()]
param([int]$Width=1920,[int]$Height=1080,[switch]$RibbonEvidence,
 [string]$Map='/Game/Hansa/World/HansaWorld_20260918/L_HansaWorld_WP')
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context=Get-HansaBuildContext
$artifact=New-HansaArtifactDirectory -Context $context -Operation "land-overlay-$Width-$Height"
$log=Join-Path $artifact 'Unreal.log'
$arguments=@($context.ProjectFile,$Map,'-game','-unattended','-nop4','-nosplash','-NoSound','-windowed','-ForceRes',"-ResX=$Width","-ResY=$Height",'-RenderOffscreen','-DisablePlugins=ModelContextProtocol','-ddc=NoZenLocalFallback','-ExecCmds=Automation RunTests Hansa.World.LandOverlayCapture.RealViewport','-TestExit=Automation Test Queue Empty',"-AbsLog=$log")
if($RibbonEvidence){$arguments += '-LandOverlayRibbonCapture'}
$arguments=$arguments -replace '^-ExecCmds=Automation','-ExecCmds=t.MaxFPS 30,Automation'
$previousSdk=[Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
 [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
 Invoke-HansaNativeCommand -FilePath (Join-Path (Split-Path $context.UnrealEditorCommand) 'UnrealEditor-Win64-DebugGame-Cmd.exe') -Arguments $arguments -LogPath (Join-Path $artifact 'Command.log') -FailureMessage 'Land viewport check failed' | Out-Null
} finally { [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$previousSdk,'Process') }
$result=Get-Content -Raw -LiteralPath $log
if($result -notmatch 'Test Completed\. Result=\{Success\} Name=\{RealViewport\}' -or $result -match 'Result=\{Fail'){throw "Land test failed: $log"}
Write-Output "Land capture passed: $artifact"
