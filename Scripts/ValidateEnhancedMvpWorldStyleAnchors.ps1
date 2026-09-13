[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$manifestPath = Join-Path $projectRoot 'Tests/Golden/enhanced_mvp_world_style_anchors_v1.json'
$projectionPath = Join-Path $projectRoot 'Source/Hansa/Private/World/HansaBuildingWorldProjection.cpp'
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json

function Assert-Contract([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

Assert-Contract ($manifest.schemaVersion -eq 1) 'World-style contract schemaVersion must be 1.'
Assert-Contract ($manifest.worldScale.gridCellCm -eq 400) 'World-style grid must remain 400 cm.'
Assert-Contract (-not $manifest.worldScale.runtimeAutoFitAllowed) 'Runtime auto-fit must remain forbidden.'
Assert-Contract (($manifest.worldScale.authoredRuntimeScale -join ',') -eq '1,1,1') 'Authored runtime scale must remain identity.'

$requiredFamilies = @('merchant-house', 'bakery', 'mill', 'road', 'harbor')
$observedFamilies = @($manifest.candidates | ForEach-Object family)
Assert-Contract ($observedFamilies.Count -eq $requiredFamilies.Count) 'Contract must contain exactly five audited families.'
foreach ($family in $requiredFamilies) {
    Assert-Contract ($observedFamilies -contains $family) "World-style contract is missing $family."
}

$allowedDispositions = @('production-ready', 'requires-revision', 'rejected')
$computed = @{
    'production-ready' = @()
    'requires-revision' = @()
    'rejected' = @()
}
foreach ($candidate in $manifest.candidates) {
    Assert-Contract ($allowedDispositions -contains $candidate.disposition) "$($candidate.family) has an invalid disposition."
    Assert-Contract ($candidate.intendedCanonicalPath.StartsWith('/Game/')) "$($candidate.family) canonical target must be in /Game/."
    foreach ($prefix in $manifest.canonicalPaths.forbiddenPrefixes) {
        Assert-Contract (-not $candidate.intendedCanonicalPath.StartsWith($prefix)) "$($candidate.family) canonical target uses forbidden prefix $prefix."
    }
    if ($candidate.sourceBlend -ne 'none') {
        Assert-Contract (Test-Path -LiteralPath (Join-Path $projectRoot $candidate.sourceBlend) -PathType Leaf) "$($candidate.family) sourceBlend is missing."
    }
    if ($candidate.currentReference.StartsWith('/Game/')) {
        $packagePath = ($candidate.currentReference -split '\.')[0]
        $assetFile = Join-Path $projectRoot ('Content/' + $packagePath.Substring(6) + '.uasset')
        Assert-Contract (Test-Path -LiteralPath $assetFile -PathType Leaf) "$($candidate.family) current Unreal reference has no package file."
    }
    if ($candidate.PSObject.Properties.Name -contains 'blenderEvidence') {
        $evidencePath = Join-Path $projectRoot $candidate.blenderEvidence
        Assert-Contract (Test-Path -LiteralPath $evidencePath -PathType Leaf) "$($candidate.family) Blender evidence is missing."
        $evidence = Get-Content -Raw -LiteralPath $evidencePath | ConvertFrom-Json
        Assert-Contract ([bool]$evidence.passed) "$($candidate.family) Blender source audit did not pass its deterministic source checks."
    }
    if ($candidate.PSObject.Properties.Name -contains 'observedPrimaryBoundsCm') {
        $bounds = $candidate.observedPrimaryBoundsCm
        $available = $candidate.availableGroundedBoundsCm
        $fits = (($bounds[0] -le $available[0]) -and ($bounds[1] -le $available[1])) -or
            (($bounds[1] -le $available[0]) -and ($bounds[0] -le $available[1]))
        if ($candidate.disposition -eq 'production-ready') {
            Assert-Contract $fits "$($candidate.family) is marked production-ready but its grounded bounds do not fit at identity scale."
        }
        if ($candidate.family -in @('bakery', 'road')) {
            Assert-Contract (-not $fits) "$($candidate.family) audit no longer demonstrates its recorded footprint failure; revise its disposition and evidence."
        }
    }
    $computed[$candidate.disposition] += $candidate.family
}

Assert-Contract (($computed['production-ready'] -join ',') -eq ($manifest.summary.productionReady -join ',')) 'productionReady summary is stale.'
Assert-Contract (($computed['requires-revision'] -join ',') -eq ($manifest.summary.requiresRevision -join ',')) 'requiresRevision summary is stale.'
Assert-Contract (($computed['rejected'] -join ',') -eq ($manifest.summary.rejected -join ',')) 'rejected summary is stale.'

$projection = Get-Content -Raw -LiteralPath $projectionPath
Assert-Contract (-not $projection.Contains('double MeshScale')) 'Runtime building projection still computes a mesh-fit scale.'
Assert-Contract ($projection.Contains('BuildingPresentation->SetRelativeScale3D(FVector::OneVector)')) 'Authored actor identity-scale presentation is missing.'
Assert-Contract ($projection.Contains('Piece->SetRelativeScale3D(FVector::OneVector)')) 'Authored road-preview identity-scale presentation is missing.'

[ordered]@{
    status = 'passed'
    contractId = $manifest.contractId
    candidateCount = $manifest.candidates.Count
    productionReady = $manifest.summary.productionReady.Count
    requiresRevision = $manifest.summary.requiresRevision.Count
    rejected = $manifest.summary.rejected.Count
    runtimeAutoFitAllowed = $manifest.worldScale.runtimeAutoFitAllowed
} | ConvertTo-Json
