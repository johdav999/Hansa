[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [string]$EngineRoot,
    [string]$ArtifactsRoot
)

. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context = Get-HansaBuildContext -EngineRoot $EngineRoot -ArtifactsRoot $ArtifactsRoot
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation 'openai-authoring-acceptance'
$previousRoot = [Environment]::GetEnvironmentVariable('HANSA_AUTHORING_ACCEPTANCE_ROOT', 'Process')
$previousProposal = [Environment]::GetEnvironmentVariable('HANSA_AUTHORING_ACCEPTANCE_PROPOSAL', 'Process')
$testArguments = @{ EngineRoot = $context.EngineRoot; ArtifactsRoot = $artifactDirectory }
try {
    [Environment]::SetEnvironmentVariable('HANSA_AUTHORING_ACCEPTANCE_ROOT', $artifactDirectory, 'Process')
    [Environment]::SetEnvironmentVariable('HANSA_AUTHORING_ACCEPTANCE_PROPOSAL', $null, 'Process')
    & (Join-Path $PSScriptRoot 'RunGenerationWorkerTests.ps1') -ArtifactsRoot $artifactDirectory
    & (Join-Path $PSScriptRoot 'RunAutomationTests.ps1') @testArguments -TestFilter Hansa.Integration.Authoring.OpenAIAcceptanceFlow -SkipBuild:$SkipBuild
    $nodeCommand = Get-Command node -ErrorAction Stop
    Invoke-HansaNativeCommand -FilePath $nodeCommand.Source -Arguments @(
        (Join-Path $context.ProjectRoot 'Tools/HansaGenerationWorker/scripts/authoring-acceptance.js')
        (Join-Path $artifactDirectory 'editor-contract.json')
        $artifactDirectory
    ) -LogPath (Join-Path $artifactDirectory 'WorkerRoundTrip.log') -FailureMessage 'Persistent mock-worker authoring acceptance failed.' | Out-Null
    [Environment]::SetEnvironmentVariable('HANSA_AUTHORING_ACCEPTANCE_PROPOSAL', (Join-Path $artifactDirectory 'worker-proposal.json'), 'Process')
    & (Join-Path $PSScriptRoot 'RunAutomationTests.ps1') @testArguments -TestFilter Hansa.Integration.Authoring.OpenAIAcceptanceFlow -SkipBuild
    & (Join-Path $PSScriptRoot 'RunAutomationTests.ps1') @testArguments -TestFilter Hansa.Architecture.GenerationWorker -SkipBuild
    $editorEvidence = Get-Content -Raw -LiteralPath (Join-Path $artifactDirectory 'editor-acceptance.json') | ConvertFrom-Json
    $workerEvidence = Get-Content -Raw -LiteralPath (Join-Path $artifactDirectory 'worker-acceptance.json') | ConvertFrom-Json
    if ($editorEvidence.status -ne 'Succeeded' -or $editorEvidence.proposalSource -ne 'persistent-mock-worker' -or $workerEvidence.status -ne 'Succeeded' -or $workerEvidence.liveProviderCalls) {
        throw 'Acceptance must finish with a successful persistent mock-worker proposal and no live calls.'
    }
    Write-HansaJsonArtifact -Path (Join-Path $artifactDirectory 'result.json') -Value ([ordered]@{
        Operation = 'RunOpenAIAuthoringAcceptance'
        Status = 'Succeeded'
        LiveProviderCalls = $false
        Editor = $editorEvidence
        Worker = $workerEvidence
        CompletedUtc = [DateTime]::UtcNow.ToString('o')
    })
}
finally {
    [Environment]::SetEnvironmentVariable('HANSA_AUTHORING_ACCEPTANCE_ROOT', $previousRoot, 'Process')
    [Environment]::SetEnvironmentVariable('HANSA_AUTHORING_ACCEPTANCE_PROPOSAL', $previousProposal, 'Process')
}
Write-Output "OpenAI authoring mock acceptance succeeded. Evidence: $artifactDirectory"
