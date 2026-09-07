[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [string]$EngineRoot,
    [string]$ArtifactsRoot
)
. (Join-Path $PSScriptRoot 'HansaBuild.Common.ps1')
$context = Get-HansaBuildContext -EngineRoot $EngineRoot -ArtifactsRoot $ArtifactsRoot
$artifactDirectory = New-HansaArtifactDirectory -Context $context -Operation 'media-acceptance'
$previousRoot = [Environment]::GetEnvironmentVariable('HANSA_MEDIA_ACCEPTANCE_ROOT', 'Process')
$previousPhase = [Environment]::GetEnvironmentVariable('HANSA_MEDIA_ACCEPTANCE_PHASE', 'Process')
$node = (Get-Command node -ErrorAction Stop).Source
$script = Join-Path $context.ProjectRoot 'Tools/HansaGenerationWorker/scripts/media-acceptance.js'
$childArguments = @{ EngineRoot = $context.EngineRoot; ArtifactsRoot = $artifactDirectory }
function Invoke-MediaWorker([string]$Phase) {
    Invoke-HansaNativeCommand -FilePath $node -Arguments @($script, $Phase, $context.ProjectRoot, $artifactDirectory) `
        -LogPath (Join-Path $artifactDirectory "Worker-$Phase.log") -FailureMessage "Mock media $Phase failed." | Out-Null
}
try {
    Invoke-MediaWorker 'prepare'
    [Environment]::SetEnvironmentVariable('HANSA_MEDIA_ACCEPTANCE_ROOT', $artifactDirectory, 'Process')
    [Environment]::SetEnvironmentVariable('HANSA_MEDIA_ACCEPTANCE_PHASE', 'promote', 'Process')
    & (Join-Path $PSScriptRoot 'RunAutomationTests.ps1') @childArguments -TestFilter Hansa.Integration.Authoring.MediaAcceptanceFlow -SkipBuild:$SkipBuild -WithRendering
    Invoke-MediaWorker 'discard-jobs'
    [Environment]::SetEnvironmentVariable('HANSA_MEDIA_ACCEPTANCE_PHASE', 'verify', 'Process')
    # A second executable invocation proves native reload without the worker store.
    & (Join-Path $PSScriptRoot 'RunAutomationTests.ps1') @childArguments -TestFilter Hansa.Integration.Authoring.MediaAcceptanceFlow -SkipBuild
    $worker = Get-Content -Raw -LiteralPath (Join-Path $artifactDirectory 'worker-acceptance.json') | ConvertFrom-Json
    $promote = Get-Content -Raw -LiteralPath (Join-Path $artifactDirectory 'editor-promote.json') | ConvertFrom-Json
    $verify = Get-Content -Raw -LiteralPath (Join-Path $artifactDirectory 'editor-verify.json') | ConvertFrom-Json
    if ($worker.status -ne 'Succeeded' -or $worker.liveProviderCalls -or $promote.status -ne 'Succeeded' -or $verify.status -ne 'Succeeded' -or -not $verify.freshEditorWithoutWorkerState -or $verify.items.Count -ne 3) {
        throw 'Media acceptance requires three promoted roles, fresh Editor verification, and no live calls.'
    }
    $plan = Get-Content -Raw -LiteralPath (Join-Path $artifactDirectory 'media-plan.json') | ConvertFrom-Json
    foreach ($item in $plan.items) {
        $sourceDirectory = Split-Path (Join-Path $context.ProjectRoot $item.descriptorPath) -Parent
        Copy-Item -LiteralPath $sourceDirectory -Destination (Join-Path $artifactDirectory "retained-$($item.kind)") -Recurse
    }
    Write-HansaJsonArtifact -Path (Join-Path $artifactDirectory 'result.json') -Value ([ordered]@{
        Operation = 'RunMediaAcceptance'; Status = 'Succeeded'; LiveProviderCalls = $false
        Worker = $worker; EditorPromotion = $promote; EditorReload = $verify
        HumanReview = 'Not proven by automation; test-only approval and audio playback surrogates'
        ShippingScope = 'Separate exclusion and cooked-content gates; not claimed by this runner'
        CompletedUtc = [DateTime]::UtcNow.ToString('o')
    })
}
catch {
    Write-HansaJsonArtifact -Path (Join-Path $artifactDirectory 'result.json') -Value ([ordered]@{
        Operation = 'RunMediaAcceptance'; Status = 'Failed'; LiveProviderCalls = $false
        Error = $_.Exception.Message; CompletedUtc = [DateTime]::UtcNow.ToString('o')
    })
    throw
}
finally {
    [Environment]::SetEnvironmentVariable('HANSA_MEDIA_ACCEPTANCE_ROOT', $previousRoot, 'Process')
    [Environment]::SetEnvironmentVariable('HANSA_MEDIA_ACCEPTANCE_PHASE', $previousPhase, 'Process')
    if (Test-Path -LiteralPath (Join-Path $artifactDirectory 'media-plan.json')) { Invoke-MediaWorker 'cleanup' }
}
Write-Output "Mock media acceptance succeeded. Evidence: $artifactDirectory"
