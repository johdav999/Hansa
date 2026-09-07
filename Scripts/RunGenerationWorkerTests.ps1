[CmdletBinding()]
param(
    [string]$ArtifactsRoot
)

. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')

$projectRoot = Get-HansaProjectRoot
$resolvedArtifactsRoot = if ([string]::IsNullOrWhiteSpace($ArtifactsRoot)) {
    Join-Path $projectRoot 'Saved\BuildArtifacts'
}
else {
    [System.IO.Path]::GetFullPath($ArtifactsRoot)
}
New-Item -ItemType Directory -Force -Path $resolvedArtifactsRoot | Out-Null
$artifactContext = [pscustomobject]@{ ArtifactsRoot = $resolvedArtifactsRoot }
$artifactDirectory = New-HansaArtifactDirectory -Context $artifactContext -Operation 'generation-worker-contract-tests'
$logPath = Join-Path $artifactDirectory 'NodeTests.log'
$resultPath = Join-Path $artifactDirectory 'result.json'

$nodeCommand = Get-Command node -ErrorAction SilentlyContinue
if ($null -eq $nodeCommand -or -not (Test-Path -LiteralPath $nodeCommand.Source -PathType Leaf)) {
    throw 'Node.js 22 or newer is required to test HansaGenerationWorker.'
}
$nodeVersionText = (& $nodeCommand.Source --version).Trim()
$nodeMajorVersion = [int]($nodeVersionText.TrimStart('v').Split('.')[0])
if ($nodeMajorVersion -lt 22) {
    throw "HansaGenerationWorker requires Node.js 22 or newer; found $nodeVersionText."
}

Push-Location (Join-Path $projectRoot 'Tools\HansaGenerationWorker')
try {
    Invoke-HansaNativeCommand `
        -FilePath $nodeCommand.Source `
        -Arguments @('--test') `
        -LogPath $logPath `
        -FailureMessage 'HansaGenerationWorker contract tests failed.' | Out-Null
}
finally {
    Pop-Location
}

Write-HansaJsonArtifact -Path $resultPath -Value ([ordered]@{
    Operation = 'RunGenerationWorkerTests'
    Status = 'Succeeded'
    ProtocolVersion = '1.0'
    Provider = 'mock'
    LiveProviderCalls = $false
    NodeVersion = $nodeVersionText
    CompletedUtc = [DateTime]::UtcNow.ToString('o')
})

Write-Output "Generation worker contract tests succeeded. Evidence: $resultPath"
