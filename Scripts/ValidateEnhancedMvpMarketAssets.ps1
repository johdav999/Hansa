[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$p15Repo = Split-Path $PSScriptRoot -Parent
$p15Contract = Get-Content -Raw (Join-Path $p15Repo 'Tests/Golden/enhanced_mvp_market_asset_family_v1.json') | ConvertFrom-Json
$p15Source = Join-Path $p15Repo $p15Contract.sourcePackage
$p15Export = Get-Content -Raw (Join-Path $p15Source 'exports/export-manifest.json') | ConvertFrom-Json
$p15Unreal = Get-Content -Raw (Join-Path $p15Source 'evidence/unreal-meshes.json') | ConvertFrom-Json
$p15Delivery = Get-Content -Raw (Join-Path $p15Source 'evidence/delivery-verification.json') | ConvertFrom-Json
$p15Audit = Get-Content -Raw (Join-Path $p15Source 'evidence/final-unreal-audit.json') | ConvertFrom-Json
if ($p15Contract.roles.Count -ne 6 -or $p15Contract.playableCoverageVerified -or $p15Export.revision -ne 5) { throw 'Unexpected role, revision or approval state.' }
if ($p15Delivery.packedImages.Count -lt 27 -or $p15Delivery.courtHullCount -ne 5 -or !$p15Delivery.exportNormalsRecalculated) { throw 'Missing source/collision proof.' }
if ($p15Delivery.positiveSourcePanVolumes -ne 682) { throw 'Editable source roof winding is not verified.' }
if (($p15Export.bakeDimensions -join ',') -ne '1024,1024' -or ($p15Export.nativeColorInputDimensions -join ',') -ne '1254,1254') { throw 'Unexpected native image dimensions.' }
foreach ($p15Role in $p15Contract.roles) {
    $p15Mesh = $p15Unreal.($p15Role.mesh)
    $p15Module = $p15Export.modules.($p15Role.mesh)
    if (!$p15Mesh -or !$p15Module -or !$p15Mesh.mesh.refPath.StartsWith($p15Contract.stagedRoot + '/Meshes/')) { throw 'Missing staged module.' }
    if ($p15Mesh.lodCount -ne 3 -or !$p15Mesh.simpleCollision -or $p15Mesh.nanite -or !$p15Mesh.slots.Count) { throw 'LOD/collision/material failure.' }
    if (($p15Mesh.triangles -join ',') -ne ($p15Role.lodTriangles -join ',')) { throw 'Imported triangle budget changed.' }
    if (($p15Module.scale -join ',') -ne '1,1,1' -or ($p15Module.location -join ',') -ne '0,0,0') { throw 'Invalid export origin/scale.' }
    if ([Math]::Abs($p15Mesh.bounds.min.z) -gt 2) { throw 'Ground pivot exceeds tolerance.' }
    $p15Package = $p15Mesh.mesh.refPath.Split('.')[0].Replace('/Game/', 'Content/') + '.uasset'
    if (!(Test-Path -LiteralPath (Join-Path $p15Repo $p15Package))) { throw "Missing imported asset: $p15Package" }
}
foreach ($p15Axis in 'x','y') {
    if ($p15Audit.bounds.min.$p15Axis -lt -580 -or $p15Audit.bounds.max.$p15Axis -gt 580) { throw 'Assembly exceeds 3x3 plot inset.' }
}
if ($p15Audit.definition.PresentationActorClass -ne 'None' -or !$p15Audit.definition.bRequiresRoad) { throw 'Unexpected production binding or road contract.' }
if (!$p15Contract.productionBound -and (Get-FileHash -LiteralPath (Join-Path $p15Repo 'Content/Hansa/Core/Buildings/DA_Building_Market.uasset') -Algorithm SHA256).Hash -ne $p15Contract.definitionSha256) { throw 'Production definition changed; approval audit required.' }
foreach ($p15File in $p15Delivery.files.PSObject.Properties) {
    if ((Get-FileHash -LiteralPath (Join-Path $p15Source $p15File.Name) -Algorithm SHA256).Hash -ne $p15File.Value.sha256) { throw "Checksum mismatch: $($p15File.Name)" }
}
if ($p15Delivery.files.PSObject.Properties.Name.Count -ne 44) { throw 'Incomplete six-module delivery.' }
$p15Config = Get-Content -Raw (Join-Path $p15Repo 'Config/DefaultGame.ini')
if (!$p15Config.Contains('+DirectoriesToNeverCook=(Path="/Game/Hansa/Generated/Staging")')) { throw 'Missing staging cook exclusion.' }
& (Join-Path $PSScriptRoot 'ValidateEnhancedMvpPromotion.ps1') -Prompt P15
