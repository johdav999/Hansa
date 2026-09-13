[CmdletBinding()]
param(
    [switch]$Create,
    [string]$EngineRoot
)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context = Get-HansaBuildContext -EngineRoot $EngineRoot
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation 'p30-stage-world'
# Explicit, pinned staging-only authoring. BuildCandidate refuses an existing destination.
# Every process has its own clean editor; no connected editor session is closed or saved.
$operations = if ($Create) { @('BuildCandidate','ReviseCandidate','BakeCandidate') } else { @('ReviseCandidate','BakeCandidate') }
$previousSdkSetting = [Environment]::GetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','Process')
try {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP','1','Process')
    foreach ($operation in $operations) {
        $unrealLog = Join-Path $artifactDirectory "$operation.log"
        Invoke-HansaNativeCommand -FilePath $context.UnrealEditorCommand -Arguments @(
            $context.ProjectFile, '-unattended', '-nop4', '-nosplash', '-NoSound', '-RenderOffscreen', '-P30Authoring',
            "-ExecCmds=Automation RunTests Hansa.World.LubeckArt.$operation;Quit",
            '-TestExit=Automation Test Queue Empty', "-AbsLog=$unrealLog"
        ) -LogPath (Join-Path $artifactDirectory "$operation-command.log") -FailureMessage "P30 $operation failed" | Out-Null
        $log = Get-Content -Raw -LiteralPath $unrealLog
        if ($log -notmatch 'TEST COMPLETE\. EXIT CODE: 0' -or $log -match 'Test Completed\. Result=\{Fail') {
            throw "P30 $operation did not pass: $unrealLog"
        }
    }
} finally {
    [Environment]::SetEnvironmentVariable('UE_SKIP_UBT_SDK_SETUP',$previousSdkSetting,'Process')
}
Write-Output "P30 staged candidate saved. Production acceptance remains open. Artifacts: $artifactDirectory"
