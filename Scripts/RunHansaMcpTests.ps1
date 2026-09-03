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
$artifactDirectory = New-HansaArtifactDirectory -Context $artifactContext -Operation 'hansa-mcp-contract-tests'
$resultPath = Join-Path $artifactDirectory 'result.json'
$logPath = Join-Path $artifactDirectory 'NodeTests.log'

$nodeCommand = Get-Command node -ErrorAction SilentlyContinue
if ($null -eq $nodeCommand -or -not (Test-Path -LiteralPath $nodeCommand.Source -PathType Leaf)) {
    throw 'Node.js 22 or newer is required to run Tools/HansaMcp contract tests.'
}
$nodeVersionText = (& $nodeCommand.Source --version).Trim()
$nodeMajorVersion = [int]($nodeVersionText.TrimStart('v').Split('.')[0])
if ($nodeMajorVersion -lt 22) {
    throw "Tools/HansaMcp requires Node.js 22 or newer; found $nodeVersionText."
}

Push-Location (Join-Path $projectRoot 'Tools\HansaMcp')
try {
    Invoke-HansaNativeCommand `
        -FilePath $nodeCommand.Source `
        -Arguments @('--test') `
        -LogPath $logPath `
        -FailureMessage 'HansaMcp contract tests failed.' | Out-Null
}
finally {
    Pop-Location
}

Write-HansaJsonArtifact -Path $resultPath -Value ([ordered]@{
    Operation = 'RunHansaMcpTests'
    Status = 'Succeeded'
    NodeVersion = $nodeVersionText
    TestCommand = 'node --test'
    LiveGameRequired = $false
    CompletedUtc = [DateTime]::UtcNow.ToString('o')
})

Write-Output "HansaMcp contract tests succeeded. Evidence: $resultPath"
