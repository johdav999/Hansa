[CmdletBinding()]
param(
    [int]$Width = 1280,
    [int]$Height = 720,
    [ValidateSet("Development", "DebugGame")][string]$Configuration = "DebugGame",
    [string]$EngineRoot
)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context = Get-HansaBuildContext -EngineRoot $EngineRoot
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation "topmenu-native-$Width-$Height"
$unrealLog = Join-Path $artifactDirectory 'Unreal.log'
New-Item -ItemType Directory -Force (Join-Path $context.ProjectRoot 'Saved/TopMenu') | Out-Null
$arguments = @($context.ProjectFile, '/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP',
    '-game', '-unattended', '-nop4', '-nosplash', '-NoSound', '-windowed', '-ForceRes',
    "-ResX=$Width", "-ResY=$Height", '-RenderOffscreen',
    '-ExecCmds=Automation RunTests Hansa.UI.TopMenu.RealViewport',
    '-TestExit=Automation Test Queue Empty', "-AbsLog=$unrealLog")
$previousSkipSdk = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
    $captureExecutable = if ($Configuration -eq 'DebugGame') { Join-Path (Split-Path $context.UnrealEditorCommand) 'UnrealEditor-Win64-DebugGame-Cmd.exe' } else { $context.UnrealEditorCommand }
    Invoke-HansaNativeCommand -FilePath $captureExecutable -Arguments $arguments `
        -LogPath (Join-Path $artifactDirectory 'Command.log') -FailureMessage 'Top menu native capture failed.' | Out-Null
} finally {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$previousSkipSdk,'Process')
}
$log = Get-Content -Raw -LiteralPath $unrealLog
if ($log -notmatch 'Test Completed\. Result=\{Success\} Name=\{RealViewport\}' -or $log -match 'Result=\{Fail' -or
    $log -notmatch 'Found 1 automation tests') { throw "Top menu capture did not pass: $unrealLog" }
Write-Output "Top menu native capture passed: $artifactDirectory"


