[CmdletBinding()]
param(
    [int]$Width = 1920,
    [int]$Height = 1080,
    [ValidatePattern("^[A-Za-z0-9_.]+$")]
    [string]$TestFilter = "Hansa.UI.GuiRepair.RealViewport",
    [string]$EngineRoot
)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context = Get-HansaBuildContext -EngineRoot $EngineRoot
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation "imagegen-$TestFilter-$Width-$Height"
$unrealLog = Join-Path $artifactDirectory 'Unreal.log'
foreach ($folder in @('TopMenu','P29','P28','ProductionInspector','ResidenceInspector')) { New-Item -ItemType Directory -Force (Join-Path $context.ProjectRoot ('Saved/'+$folder)) | Out-Null }
$arguments = @($context.ProjectFile, '/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP',
    '-game', '-unattended', '-nop4', '-nosplash', '-NoSound', '-windowed', '-ForceRes',
    "-ResX=$Width", "-ResY=$Height", '-RenderOffscreen',
    "-ExecCmds=Automation RunTests $TestFilter",
    '-TestExit=Automation Test Queue Empty', "-AbsLog=$unrealLog")
$previousSkipSdk = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
    Invoke-HansaNativeCommand -FilePath (Join-Path (Split-Path $context.UnrealEditorCommand) 'UnrealEditor-Win64-DebugGame-Cmd.exe') -Arguments $arguments `
        -LogPath (Join-Path $artifactDirectory 'Command.log') -FailureMessage 'GUI artwork native capture failed.' | Out-Null
} finally {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$previousSkipSdk,'Process')
}
$log = Get-Content -Raw -LiteralPath $unrealLog
if ($log -notmatch 'Test Completed\. Result=\{Success\} Name=\{RealViewport\}' -or $log -match 'Result=\{Fail' -or
    $log -notmatch 'Found 1 automation tests') { throw "GUI artwork capture did not pass: $unrealLog" }
Write-Output "GUI artwork native capture passed: $artifactDirectory"


