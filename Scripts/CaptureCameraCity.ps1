[CmdletBinding()]
param([int]$Width=1920,[int]$Height=1080,
 [ValidateSet('DebugGame','Development')][string]$Configuration='DebugGame')
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context=Get-HansaBuildContext
$artifact=New-HansaArtifactDirectory -Context $context -Operation "camera-city-$Configuration-$Width-$Height"
$log=Join-Path $artifact 'Unreal.log'
$arguments=@($context.ProjectFile,'/Game/Hansa/World/HansaWorld_20260918/L_HansaWorld_WP','-game','-unattended','-nop4','-nosplash','-NoSound','-windowed','-ForceRes',"-ResX=$Width","-ResY=$Height",'-RenderOffscreen','-DisablePlugins=ModelContextProtocol','-ExecCmds=Automation RunTests Hansa.UI.HUD.CameraCity.RealViewport','-TestExit=Automation Test Queue Empty',"-AbsLog=$log")
$previousSdk=[Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
 [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
 $executable=if($Configuration -eq 'Development'){'UnrealEditor-Cmd.exe'}else{'UnrealEditor-Win64-DebugGame-Cmd.exe'}
 Invoke-HansaNativeCommand -FilePath (Join-Path (Split-Path $context.UnrealEditorCommand) $executable) -Arguments $arguments -LogPath (Join-Path $artifact 'Command.log') -FailureMessage 'Camera city viewport check failed' | Out-Null
} finally { [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$previousSdk,'Process') }
$result=Get-Content -Raw -LiteralPath $log
if($result -notmatch 'Test Completed\. Result=\{Success\} Name=\{RealViewport\}' -or $result -match 'Result=\{Fail'){throw "Camera city test failed: $log"}
Write-Output "Camera city capture passed: $artifact"
