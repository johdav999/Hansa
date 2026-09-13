[CmdletBinding()]
param(
    [int]$Width = 1920,
    [int]$Height = 1080,
    [string]$EngineRoot,
    [string]$ArtifactsRoot
)

. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')

$context = Get-HansaBuildContext -EngineRoot $EngineRoot -ArtifactsRoot $ArtifactsRoot
$artifacts = New-HansaArtifactDirectory -Context $context -Operation "visible-wagons-$Width-$Height"
$unrealLog = Join-Path $artifacts 'Unreal.log'
$arguments = @(
    $context.ProjectFile,
    '/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP',
    '-game',
    '-unattended',
    '-nop4',
    '-nosplash',
    '-NoSound',
    '-windowed',
    '-ForceRes',
    "-ResX=$Width",
    "-ResY=$Height",
    '-RenderOffscreen',
    '-ExecCmds=Automation RunTests Hansa.Integration.VisibleWagon.RealViewport',
    '-TestExit=Automation Test Queue Empty',
    "-AbsLog=$unrealLog"
)
$previousSdk = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', 'Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', '1', 'Process')
    Invoke-HansaNativeCommand -FilePath $context.UnrealEditorCommand -Arguments $arguments -LogPath (Join-Path $artifacts 'Command.log') -FailureMessage 'Visible wagon capture failed.' | Out-Null
}
finally {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP', $previousSdk, 'Process')
}
$log = Get-Content -Raw -LiteralPath $unrealLog
if ($log -notmatch 'Test Completed\. Result=\{Success\} Name=\{RealViewport\}' -or $log -match 'Result=\{Fail') {
    throw "Visible wagon capture did not pass: $unrealLog"
}
$screenshotName = 'local-wagons-{0}x{1}.png' -f $Width, $Height
$farmScreenshotName = 'local-wagons-{0}x{1}-farm-to-market.png' -f $Width, $Height
$evidenceName = 'local-wagons-{0}x{1}.txt' -f $Width, $Height
$farmEvidenceName = 'local-wagons-{0}x{1}-farm-to-market.txt' -f $Width, $Height
Write-HansaJsonArtifact -Path (Join-Path $artifacts 'result.json') -Value ([ordered]@{
    Operation = 'VisibleWagonCapture'
    Status = 'Succeeded'
    Width = $Width
    Height = $Height
    Screenshot = Join-Path $context.ProjectRoot (Join-Path 'Saved\VisibleWagons' $screenshotName)
    FarmToMarketScreenshot = Join-Path $context.ProjectRoot (Join-Path 'Saved\VisibleWagons' $farmScreenshotName)
    Evidence = Join-Path $context.ProjectRoot (Join-Path 'Saved\VisibleWagons' $evidenceName)
    FarmToMarketEvidence = Join-Path $context.ProjectRoot (Join-Path 'Saved\VisibleWagons' $farmEvidenceName)
    CompletedUtc = [DateTime]::UtcNow.ToString('o')
    Log = $unrealLog
})
Write-Output "Visible wagon capture passed: $artifacts"
