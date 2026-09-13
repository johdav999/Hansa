param([switch]$RequireProduction)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$package = Join-Path $repo 'SourceArt/Generated/Buildings/HansaBakery_P10_20260907'
$manifest = Get-Content (Join-Path $package 'delivery_manifest.json') -Raw | ConvertFrom-Json
foreach ($row in $manifest.files) {
    $path = [IO.Path]::GetFullPath((Join-Path $package $row.path))
    if (-not $path.StartsWith($package + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw "Escaping artifact path: $($row.path)" }
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing artifact: $($row.path)" }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $row.sha256) { throw "Changed artifact: $($row.path)" }
}
$audit = Get-Content (Join-Path $package 'evidence/source_audit.json') -Raw | ConvertFrom-Json
$original = Join-Path $repo 'SourceArt/Generated/Buildings/HansaBakery_ImageGen_20260906_02/HansaBakery.blend'
if ((Get-FileHash -LiteralPath $original -Algorithm SHA256).Hash -ne $audit.sha256) { throw 'Original bakery source changed.' }
$meshes = Get-Content (Join-Path $package 'evidence/unreal_meshes.json') -Raw | ConvertFrom-Json
foreach ($role in 'Body','Input','Output','Sign','Construction') {
    $mesh = $meshes.$role
    if ($mesh.lodTriangles.Count -ne 3 -or $mesh.lodTriangles[0] -le $mesh.lodTriangles[1] -or $mesh.lodTriangles[1] -le $mesh.lodTriangles[2]) { throw "$role LOD contract failed." }
    if (-not $mesh.path.StartsWith('/Game/Mesh/hansa-bakery/P10/')) { throw "$role path is not canonical." }
    foreach ($material in $mesh.materials) { if (-not $material.StartsWith('/Game/Mesh/hansa-bakery/')) { throw "Unverified material: $material" } }
}
if ($meshes.Body.simpleCollisionCount -ne 2) { throw 'Expected two plot-bounded body collision boxes.' }
$density = Get-Content (Join-Path $package 'evidence/measured_density.json') -Raw | ConvertFrom-Json
foreach ($family in $density.PSObject.Properties) { if ($family.Value.trianglesBelow384 -ne 0) { throw "Insufficient measured density: $($family.Name)" } }
$reopen = Get-Content (Join-Path $package 'evidence/unreal_reopen.json') -Raw | ConvertFrom-Json
if (-not $reopen.savedAndReopened) { throw 'No saved/reopened engine evidence.' }
foreach ($path in $reopen.candidateDependencyClosure) { if ($path -match '/Staging/|/Developer|/Engine/BasicShapes/') { throw "Draft dependency: $path" } }
if ($RequireProduction) { throw 'P10 is approved and bound under catalog v4, but correlated gameplay-resolution and Shipping acceptance remain pending.' }
Write-Output 'P10 source integrity, LOD, material, source-preservation and density checks PASS. See the approved promotion receipt for binding; full release acceptance remains pending.'
