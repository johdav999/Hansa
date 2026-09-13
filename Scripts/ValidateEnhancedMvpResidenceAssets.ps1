[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$p14Repo = Split-Path $PSScriptRoot -Parent
$p14Contract = Get-Content -Raw (Join-Path $p14Repo 'Tests/Golden/enhanced_mvp_residence_asset_family_v1.json') | ConvertFrom-Json
$p14Source = Join-Path $p14Repo $p14Contract.sourcePackage
$p14Export = Get-Content -Raw (Join-Path $p14Source 'exports/export-manifest.json') | ConvertFrom-Json
$p14Unreal = Get-Content -Raw (Join-Path $p14Source 'evidence/unreal-meshes.json') | ConvertFrom-Json
$p14Delivery = Get-Content -Raw (Join-Path $p14Source 'evidence/delivery-verification.json') | ConvertFrom-Json
if ($p14Contract.roles.Count -ne 4 -or $p14Export.revision -ne $p14Contract.revision) { throw 'Unexpected role, revision, or approval state.' }
if ($p14Delivery.outwardPanCount -ne 3536 -or $p14Delivery.packedImages.Count -lt 24) { throw 'Missing packed source or outward-normal proof.' }
if (($p14Export.bakeDimensions -join ',') -ne '1024,1024' -or ($p14Export.nativeColorInputDimensions -join ',') -ne '1254,1254') { throw 'Unexpected native texture dimensions.' }
foreach ($p14Role in $p14Contract.roles) {
    $p14Mesh = $p14Unreal.($p14Role.mesh)
    $p14Module = $p14Export.modules.($p14Role.mesh)
    if (!$p14Mesh -or !$p14Module) { throw "Missing role $($p14Role.mesh)" }
    if (!$p14Mesh.mesh.refPath.StartsWith($p14Contract.stagedRoot + '/' + $p14Contract.selectedMeshFolder + '/')) { throw 'Wrong mesh revision.' }
    if ($p14Mesh.lodCount -ne 3 -or !$p14Mesh.simpleCollision -or $p14Mesh.nanite -or $p14Mesh.slots.Count -ne 7) { throw "LOD/collision/material failure: $($p14Role.mesh)" }
    if ($p14Mesh.triangles[0] -ne $p14Role.lod0Triangles -or $p14Module.triangles -ne $p14Role.lod0Triangles) { throw 'Triangle mismatch.' }
    if (($p14Module.scale -join ',') -ne '1,1,1' -or ($p14Module.location -join ',') -ne '0,0,0') { throw 'Scale compensation or off-origin export.' }
    foreach ($p14Axis in @('x','y')) {
        if ($p14Mesh.bounds.min.$p14Axis -lt -380 -or $p14Mesh.bounds.max.$p14Axis -gt 380) { throw 'Residence exceeds unchanged parcel inset.' }
    }
    if ([Math]::Abs($p14Mesh.bounds.min.z) -gt 2) { throw 'Ground pivot exceeds tolerance.' }
}
foreach ($p14File in $p14Delivery.files.PSObject.Properties) {
    $p14Path = Join-Path $p14Source $p14File.Name
    if ((Get-FileHash -LiteralPath $p14Path -Algorithm SHA256).Hash -ne $p14File.Value.sha256) { throw "Checksum mismatch: $($p14File.Name)" }
}
if ($p14Delivery.files.PSObject.Properties.Name.Count -ne 37) { throw 'Incomplete delivery evidence.' }
& (Join-Path $PSScriptRoot 'ValidateEnhancedMvpPromotion.ps1') -Prompt P14
