[CmdletBinding()]
param([int]$Width=1280,[int]$Height=720,[float]$Scale=1)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context=Get-HansaBuildContext
$artifact=New-HansaArtifactDirectory -Context $context -Operation "route-cargo-slots-$Width-$Height-$Scale"
$log=Join-Path $artifact 'Unreal.log'
$arguments=@($context.ProjectFile,'/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP','-game','-unattended','-nop4','-nosplash','-NoSound','-windowed','-ForceRes',"-ResX=$Width","-ResY=$Height","-HansaGuiScale=$Scale",'-RenderOffscreen','-DisablePlugins=ModelContextProtocol','-ExecCmds=Automation RunTests Hansa.TradeRoute.RealViewport','-TestExit=Automation Test Queue Empty',"-AbsLog=$log")
$previousSdk=[Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
 [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
 Invoke-HansaNativeCommand -FilePath $context.UnrealEditorCommand -Arguments $arguments -LogPath (Join-Path $artifact 'Command.log') -FailureMessage 'Route cargo viewport check failed' | Out-Null
} finally { [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$previousSdk,'Process') }
$result=Get-Content -Raw -LiteralPath $log
if($result -notmatch 'Test Completed\. Result=\{Success\} Name=\{RealViewport\}' -or $result -match 'Result=\{Fail'){throw "Route cargo test failed: $log"}
Write-Output "Route cargo capture passed: $artifact"
