[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [switch]$Candidate,
    [switch]$Verify,
    [ValidateRange(720,7680)][int]$Width = 1920,
    [ValidateRange(600,4320)][int]$Height = 1080,
    [ValidateRange(0.8,1.4)][double]$UiScale = 1.0,
    [switch]$LargeText
)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context = Get-HansaBuildContext
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'Build.ps1') -Target HansaEditor -Configuration Development }
$artifacts = New-HansaArtifactDirectory -Context $context -Operation 'textile-playable-review'
$arguments = @(
    $context.ProjectFile,
    '/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/L_Lubeck_Terrain_Preview_WP',
    '-game', '-nop4', '-nosplash',
    '-forcerhibypass', '-norhithread', '-windowed', "-ResX=$Width", "-ResY=$Height",
    "-TextileUIScale=$($UiScale.ToString([Globalization.CultureInfo]::InvariantCulture))",
    '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False',
    "-AbsLog=$(Join-Path $artifacts 'Unreal.log')"
)
if ($Candidate) { $arguments += '-TextileProductionCandidate' } else { $arguments += '-TextileProductionApprovedReview' }
if ($LargeText) { $arguments += '-TextileLargeText' }
if ($Verify) { $arguments += @('-unattended', '-ExecCmds=Automation RunTests Hansa.UI.TextileProduction.PlayableReview;Quit', '-TestExit=Automation Test Queue Empty') }
$previous = $env:UE_SKIP_UBT_SDK_SETUP
try {
    $env:UE_SKIP_UBT_SDK_SETUP = '1'
    Invoke-HansaNativeCommand -FilePath $context.UnrealEditorCommand -Arguments $arguments -LogPath (Join-Path $artifacts 'Command.log') -FailureMessage 'Textile review failed' | Out-Null
} finally { $env:UE_SKIP_UBT_SDK_SETUP = $previous }
if ($Verify) {
    $log = Get-Content -Raw (Join-Path $artifacts 'Unreal.log')
    if ($log -notmatch 'Found 1 automation tests' -or $log -notmatch 'TEST COMPLETE\. EXIT CODE: 0' -or $log -match 'Result=\{Fail') { throw "Review did not pass: $artifacts" }
}
Write-Output "Textile review evidence: $artifacts; screenshots: Saved/TextileProduction"