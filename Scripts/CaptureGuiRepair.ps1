[CmdletBinding()]
param(
    [ValidateSet('Production','P30Candidate')][string]$World = 'Production',
    [switch]$P31Candidate,
    [switch]$P33Candidate,
    [switch]$NoZenDdc,
    [switch]$ReadOnlyZenDdc,
    [int]$Width = 1920,
    [int]$Height = 1080,
    [ValidateRange(0.8,1.4)][float]$UiScale = 1.0,
    [string]$TestFilter = 'Hansa.UI.GuiRepair.RealViewport',
    [ValidateSet('Development','DebugGame')][string]$Configuration = 'Development',
    [string]$EngineRoot
)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context = Get-HansaBuildContext -EngineRoot $EngineRoot
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation "gui-repair-$Width-$Height"
$unrealLog = Join-Path $artifactDirectory 'Unreal.log'
$map = if ($World -eq 'P30Candidate') { '/Game/Hansa/Generated/Staging/LubeckWorldArt_P30/L_Lubeck_WorldArt_Candidate' } else { '/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP' }
$arguments = @($context.ProjectFile, $map,
    '-game', '-unattended', '-nop4', '-nosplash', '-NoSound', '-windowed', '-ForceRes',
    "-ResX=$Width", "-ResY=$Height", "-HansaGuiScale=$($UiScale.ToString([Globalization.CultureInfo]::InvariantCulture))", '-RenderOffscreen',
    "-ExecCmds=Automation RunTests $TestFilter",
    '-TestExit=Automation Test Queue Empty', "-AbsLog=$unrealLog")
if ($P31Candidate) { $arguments += '-P31Candidate' }
if ($P33Candidate) { $arguments += '-P33Candidate' }
if ($NoZenDdc) { $arguments += '-ddc=NoZenLocalFallback' }
if ($ReadOnlyZenDdc) { $arguments += '-ZenLocalDataCacheReadOnly=true' }
$executable = if ($Configuration -eq 'DebugGame') {
    Join-Path (Split-Path $context.UnrealEditorCommand) 'UnrealEditor-Win64-DebugGame-Cmd.exe'
} else { $context.UnrealEditorCommand }
$previousSkipSdk = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
    Invoke-HansaNativeCommand -FilePath $executable -Arguments $arguments `
        -LogPath (Join-Path $artifactDirectory 'Command.log') -FailureMessage 'GUI repair native capture failed.' | Out-Null
} finally {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$previousSkipSdk,'Process')
}
$log = Get-Content -Raw -LiteralPath $unrealLog
if ($log -notmatch 'Test Completed\. Result=\{Success\}' -or $log -match 'Result=\{Fail' -or
    $log -notmatch 'Found 1 automation tests') { throw "GUI capture did not pass: $unrealLog" }
Write-Output "GUI native capture passed: $artifactDirectory"
