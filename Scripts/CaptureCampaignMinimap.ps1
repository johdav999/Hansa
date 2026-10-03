[CmdletBinding()]
param([int]$Width=1920,[int]$Height=1080,[float]$Scale=1,
 [ValidateSet('DebugGame','Development')][string]$Configuration='DebugGame')
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context=Get-HansaBuildContext
$artifact=New-HansaArtifactDirectory -Context $context -Operation "campaign-minimap-$Configuration-$Width-$Height-$Scale"
$log=Join-Path $artifact 'Unreal.log'
$arguments=@($context.ProjectFile,'/Game/Hansa/World/HansaWorld_20260918/L_HansaWorld_WP','-game','-unattended','-nop4','-nosplash','-NoSound','-windowed','-ForceRes',"-ResX=$Width","-ResY=$Height","-HansaGuiScale=$Scale",'-RenderOffscreen','-DisablePlugins=ModelContextProtocol','-ExecCmds=Automation RunTests Hansa.UI.Minimap.CampaignRealViewport','-TestExit=Automation Test Queue Empty',"-AbsLog=$log")
$oldSdk=[Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
 [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
 $executable=if($Configuration -eq 'Development'){'UnrealEditor-Cmd.exe'}else{'UnrealEditor-Win64-DebugGame-Cmd.exe'}
 Invoke-HansaNativeCommand -FilePath (Join-Path (Split-Path $context.UnrealEditorCommand) $executable) -Arguments $arguments -LogPath (Join-Path $artifact 'Command.log') -FailureMessage 'Campaign minimap viewport check failed' | Out-Null
} finally { [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$oldSdk,'Process') }
$result=Get-Content -Raw -LiteralPath $log
if($result -notmatch 'Test Completed\. Result=\{Success\} Name=\{CampaignRealViewport\}' -or $result -match 'Result=\{Fail'){throw "Campaign minimap test failed: $log"}
Write-Output "Campaign minimap capture passed: $artifact"
