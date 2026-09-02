[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [ValidateSet('Win64')]
    [string]$Platform = 'Win64',
    [string]$EngineRoot,
    [string]$ArtifactsRoot
)

. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')

$context = Get-HansaBuildContext -EngineRoot $EngineRoot -ArtifactsRoot $ArtifactsRoot
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation "shipping-exclusion-$Platform"

if (-not $SkipBuild) {
    Invoke-HansaNativeCommand `
        -FilePath $context.BuildScript `
        -Arguments @(
            'Hansa'
            $Platform
            'Shipping'
            "-Project=$($context.ProjectFile)"
            '-WaitMutex'
            '-NoHotReloadFromIDE'
        ) `
        -LogPath (Join-Path $artifactDirectory 'BuildShipping.log') `
        -FailureMessage 'The Shipping build required for the exclusion audit failed.' | Out-Null
}

$platformBinaries = Join-Path $context.ProjectRoot "Binaries\$Platform"
$receiptPath = Join-Path $platformBinaries "Hansa-$Platform-Shipping.target"
$executablePath = Join-Path $platformBinaries "Hansa-$Platform-Shipping.exe"
$filesToScan = @($receiptPath, $executablePath)
$forbiddenTokens = @(
    'HansaAutomation'
    'HansaEditor'
    'HansaTests'
    'HansaMcp'
    'HansaGenerationWorker'
    'WITH_HANSA_AUTOMATION'
)

$scanResults = [System.Collections.Generic.List[object]]::new()
$failures = [System.Collections.Generic.List[string]]::new()
foreach ($file in $filesToScan) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        $failures.Add("Expected Shipping artifact does not exist: $file")
        continue
    }

    $foundTokens = @(Find-HansaAsciiTokens -Path $file -Tokens $forbiddenTokens)
    if ($foundTokens.Count -gt 0) {
        $failures.Add("Forbidden Shipping token(s) in ${file}: $($foundTokens -join ', ')")
    }

    $fileInfo = Get-Item -LiteralPath $file
    $scanResults.Add([ordered]@{
        Path = $fileInfo.FullName
        Bytes = $fileInfo.Length
        Sha256 = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
        ForbiddenTokensFound = $foundTokens
    })
}

$projectDescriptor = Get-Content -Raw -LiteralPath $context.ProjectFile | ConvertFrom-Json
$expectedHostTypes = [ordered]@{
    HansaSimulation = 'Runtime'
    Hansa = 'Runtime'
    HansaEditor = 'Editor'
    HansaAutomation = 'DeveloperTool'
    HansaTests = 'DeveloperTool'
}

foreach ($expected in $expectedHostTypes.GetEnumerator()) {
    $actualModule = $projectDescriptor.Modules | Where-Object { $_.Name -eq $expected.Key }
    if ($null -eq $actualModule -or $actualModule.Type -ne $expected.Value) {
        $failures.Add("Module '$($expected.Key)' must have host type '$($expected.Value)' in Hansa.uproject.")
    }
}

$runtimeRules = @(
    Join-Path $context.ProjectRoot 'Source\Hansa\Hansa.Build.cs'
    Join-Path $context.ProjectRoot 'Source\HansaSimulation\HansaSimulation.Build.cs'
)

foreach ($rulesPath in $runtimeRules) {
    $rulesText = Get-Content -Raw -LiteralPath $rulesPath
    foreach ($forbiddenModule in @('HansaEditor', 'HansaAutomation', 'HansaTests')) {
        if ($rulesText.Contains('"' + $forbiddenModule + '"')) {
            $failures.Add("Runtime build rules contain forbidden reverse dependency '$forbiddenModule': $rulesPath")
        }
    }
}

$resultPath = Join-Path $artifactDirectory 'result.json'
$result = [ordered]@{
    Operation = 'VerifyShippingExclusion'
    Status = if ($failures.Count -eq 0) { 'Succeeded' } else { 'Failed' }
    Scope = 'Target receipt and executable; packaged/cooked/depot audit is a later gate'
    Platform = $Platform
    ShippingBuildSkipped = [bool]$SkipBuild
    ProjectFile = $context.ProjectFile
    EngineRoot = $context.EngineRoot
    CompletedUtc = [DateTime]::UtcNow.ToString('o')
    ForbiddenTokens = $forbiddenTokens
    ScannedFiles = $scanResults
    Failures = $failures
}

Write-HansaJsonArtifact -Path $resultPath -Value $result

if ($failures.Count -gt 0) {
    throw "Shipping exclusion audit failed:`n$($failures -join [Environment]::NewLine)`nEvidence: $resultPath"
}

Write-Output "Shipping exclusion audit succeeded. Evidence: $resultPath"
