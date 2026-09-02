[CmdletBinding()]
param(
    [string]$TestFilter = 'Hansa',
    [switch]$SkipBuild,
    [string]$EngineRoot,
    [string]$ArtifactsRoot
)

. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')

if ([string]::IsNullOrWhiteSpace($TestFilter) -or $TestFilter -notmatch '^[A-Za-z0-9_.-]+$') {
    throw 'TestFilter must contain only letters, digits, dot, underscore or hyphen.'
}

$context = Get-HansaBuildContext -EngineRoot $EngineRoot -ArtifactsRoot $ArtifactsRoot
$safeFilter = $TestFilter -replace '[^A-Za-z0-9_.-]', '-'
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation "automation-$safeFilter"

if (-not $SkipBuild) {
    Invoke-HansaNativeCommand `
        -FilePath $context.BuildScript `
        -Arguments @(
            'HansaEditor'
            'Win64'
            'Development'
            "-Project=$($context.ProjectFile)"
            '-WaitMutex'
            '-NoHotReloadFromIDE'
        ) `
        -LogPath (Join-Path $artifactDirectory 'BuildEditor.log') `
        -FailureMessage 'The Editor build required for automation tests failed.' | Out-Null
}

$unrealLogPath = Join-Path $artifactDirectory 'UnrealEditor.log'
$wrapperLogPath = Join-Path $artifactDirectory 'AutomationCommand.log'
$arguments = @(
    $context.ProjectFile
    '-unattended'
    '-nop4'
    '-nosplash'
    '-NullRHI'
    '-NoSound'
    "-ExecCmds=Automation RunTests $TestFilter;Quit"
    '-TestExit=Automation Test Queue Empty'
    "-AbsLog=$unrealLogPath"
)

Invoke-HansaNativeCommand `
    -FilePath $context.UnrealEditorCommand `
    -Arguments $arguments `
    -LogPath $wrapperLogPath `
    -FailureMessage "Unreal automation tests failed for filter '$TestFilter'." | Out-Null

if (-not (Test-Path -LiteralPath $unrealLogPath -PathType Leaf)) {
    throw "Unreal exited successfully but did not create its expected log: $unrealLogPath"
}

$unrealLog = Get-Content -Raw -LiteralPath $unrealLogPath
$foundMatch = [regex]::Match($unrealLog, "Found (\d+) automation tests based on '$([regex]::Escape($TestFilter))'")
if (-not $foundMatch.Success) {
    throw "Unreal did not report a test count for filter '$TestFilter'. Review $unrealLogPath"
}

$testCount = [int]$foundMatch.Groups[1].Value
if ($testCount -le 0) {
    throw "Automation filter '$TestFilter' matched zero tests. Review $unrealLogPath"
}

if ($unrealLog -notmatch 'TEST COMPLETE\. EXIT CODE: 0') {
    throw "Unreal did not report a successful test completion marker. Review $unrealLogPath"
}

if ($unrealLog -match 'Test Completed\. Result=\{Fail') {
    throw "At least one Unreal automation test reported failure. Review $unrealLogPath"
}

Write-HansaJsonArtifact -Path (Join-Path $artifactDirectory 'result.json') -Value ([ordered]@{
    Operation = 'RunAutomationTests'
    Status = 'Succeeded'
    Filter = $TestFilter
    TestsFound = $testCount
    EditorBuildSkipped = [bool]$SkipBuild
    ProjectFile = $context.ProjectFile
    EngineRoot = $context.EngineRoot
    CompletedUtc = [DateTime]::UtcNow.ToString('o')
    UnrealLog = $unrealLogPath
    CommandLog = $wrapperLogPath
})

Write-Output "Automation tests succeeded. Filter: $TestFilter. Tests found: $testCount. Artifacts: $artifactDirectory"
