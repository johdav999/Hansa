[CmdletBinding()]
param(
    [int]$Width = 1280,
    [int]$Height = 720,
    [switch]$Accessible,
    [ValidateRange(0.8,1.4)]
    [double]$UiScale = 1.0,
    [string]$EngineRoot
)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context = Get-HansaBuildContext -EngineRoot $EngineRoot
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation "p21-native-$Width-$Height"
$unrealLog = Join-Path $artifactDirectory 'Unreal.log'
New-Item -ItemType Directory -Force (Join-Path $context.ProjectRoot 'Saved/P21') | Out-Null
$arguments = @($context.ProjectFile, '/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP',
    '-game', '-unattended', '-nop4', '-nosplash', '-NoSound', '-windowed', '-ForceRes',
    "-ResX=$Width", "-ResY=$Height", "-P21Scale=$($UiScale.ToString([Globalization.CultureInfo]::InvariantCulture))", '-RenderOffscreen',
    '-ExecCmds=Automation RunTests Hansa.UI.Style.RealViewport',
    '-TestExit=Automation Test Queue Empty', "-AbsLog=$unrealLog")
if ($Accessible) { $arguments += '-P21Accessible' }
$previousSkipSdk = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
    Invoke-HansaNativeCommand -FilePath (Join-Path (Split-Path $context.UnrealEditorCommand) 'UnrealEditor-Win64-DebugGame-Cmd.exe') -Arguments $arguments `
        -LogPath (Join-Path $artifactDirectory 'Command.log') -FailureMessage 'P21 native capture failed.' | Out-Null
} finally {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$previousSkipSdk,'Process')
}
$log = Get-Content -Raw -LiteralPath $unrealLog
if ($log -notmatch 'Test Completed\. Result=\{Success\} Name=\{RealViewport\}' -or $log -match 'Result=\{Fail' -or
    $log -notmatch 'Found 1 automation tests') { throw "P21 capture did not pass: $unrealLog" }
Write-Output "P21 native capture passed: $artifactDirectory"
