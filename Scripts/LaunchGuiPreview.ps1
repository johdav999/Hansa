[CmdletBinding()]
param(
    [ValidateRange(1280,7680)][int]$Width = 1920,
    [ValidateRange(720,4320)][int]$Height = 1080,
    [string]$EngineRoot,
    [switch]$Build,
    [switch]$P30Candidate,
    [switch]$P31Candidate,
    [switch]$P33Candidate,
    [switch]$NoZenDdc,
    [switch]$ReadOnlyZenDdc,
    [switch]$DryRun
)

. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context = Get-HansaBuildContext -EngineRoot $EngineRoot
if ($Build -and -not $DryRun) {
    & (Join-Path $PSScriptRoot 'Build.ps1') -Configuration Development -EngineRoot $context.EngineRoot
}
$guiExecutable = Join-Path (Split-Path $context.UnrealEditorCommand) 'UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $guiExecutable)) { throw "Editor executable not found: $guiExecutable" }
$previewMap = if ($P30Candidate) { '/Game/Hansa/Generated/Staging/LubeckWorldArt_P30/L_Lubeck_WorldArt_Candidate' } else { '/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/L_Lubeck_Terrain_Preview_WP' }
$guiArguments = @(
    ('"{0}"' -f $context.ProjectFile),
    $previewMap,
    '-game', '-windowed', '-ForceRes', "-ResX=$Width", "-ResY=$Height", '-nosplash'
)
if ($P31Candidate) { $guiArguments += '-P31Candidate' }
if ($P33Candidate) { $guiArguments += '-P33Candidate' }
if ($NoZenDdc) { $guiArguments += '-ddc=NoZenLocalFallback' }
if ($ReadOnlyZenDdc) { $guiArguments += '-ZenLocalDataCacheReadOnly=true' }
if ($DryRun) {
    [pscustomobject]@{ Executable=$guiExecutable; Arguments=($guiArguments -join ' '); Configuration='Development' }
    return
}
if ($P30Candidate) { Write-Warning 'P30 staged review candidate: incomplete asset prerequisites; not approved for production.' }
# This is an explicitly interactive game preview, so a visible window is intended.
$guiProcess = Start-Process -FilePath $guiExecutable -ArgumentList $guiArguments -WorkingDirectory $context.ProjectRoot -WindowStyle Normal -PassThru
Write-Output "Started Development GUI preview (PID $($guiProcess.Id)). Start at the title screen. Settings are available from the title screen or the in-game Menu."
