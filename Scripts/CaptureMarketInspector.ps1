[CmdletBinding()]
param([int]$Width=1920,[int]$Height=1080,[string]$EngineRoot)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context=Get-HansaBuildContext -EngineRoot $EngineRoot
$artifacts=New-HansaArtifactDirectory -Context $context -Operation "market-inspector-$Width-$Height"
$unrealLog=Join-Path $artifacts 'Unreal.log'
$arguments=@($context.ProjectFile,'/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP',
    '-game','-unattended','-nop4','-nosplash','-NoSound','-windowed','-ForceRes',
    "-ResX=$Width","-ResY=$Height",'-RenderOffscreen',
    '-ExecCmds=Automation RunTests Hansa.UI.MarketInspector.RealViewport',
    '-TestExit=Automation Test Queue Empty',"-AbsLog=$unrealLog")
$previousSdk=[Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
    Invoke-HansaNativeCommand -FilePath $context.UnrealEditorCommand -Arguments $arguments `
        -LogPath (Join-Path $artifacts 'Command.log') -FailureMessage 'Market inspector capture failed.' | Out-Null
} finally { [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$previousSdk,'Process') }
$log=Get-Content -Raw -LiteralPath $unrealLog
if($log -notmatch 'Test Completed\. Result=\{Success\} Name=\{RealViewport\}' -or $log -match 'Result=\{Fail') {
    throw "Market inspector capture did not pass: $unrealLog"
}
Write-Output "Market inspector capture passed: $artifacts"
