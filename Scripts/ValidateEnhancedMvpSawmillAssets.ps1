[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$p13Repo = Split-Path $PSScriptRoot -Parent
$p13Contract = Get-Content -Raw (Join-Path $p13Repo 'Tests/Golden/enhanced_mvp_sawmill_asset_family_v1.json') | ConvertFrom-Json
$p13Source = Join-Path $p13Repo $p13Contract.sourcePackage
$p13Export = Get-Content -Raw (Join-Path $p13Source 'exports/export-manifest.json') | ConvertFrom-Json
$p13Unreal = Get-Content -Raw (Join-Path $p13Source 'evidence/unreal-meshes.json') | ConvertFrom-Json
$p13Delivery = Get-Content -Raw (Join-Path $p13Source 'evidence/delivery-verification.json') | ConvertFrom-Json
if ($p13Contract.roles.Count -ne 5) { throw 'Unexpected role or approval state.' }
foreach ($p13Role in $p13Contract.roles) {
    $p13Mesh = $p13Unreal.($p13Role.mesh)
    $p13Module = $p13Export.modules.($p13Role.mesh)
    if (!$p13Mesh -or !$p13Module) { throw "Missing role $($p13Role.mesh)" }
    if ($p13Mesh.lodCount -ne 3 -or !$p13Mesh.simpleCollision) { throw "LOD/collision failure: $($p13Role.mesh)" }
    if ($p13Mesh.triangles[0] -ne $p13Role.lod0Triangles) { throw "Triangle mismatch: $($p13Role.mesh)" }
    if (($p13Module.scale -join ',') -ne '1,1,1') { throw 'Scale compensation is forbidden.' }
    foreach ($p13Axis in 0..1) {
        $p13Name = @('x','y')[$p13Axis]
        $p13Half = $p13Contract.groundedInsetCm[$p13Axis] / 2
        if ($p13Mesh.bounds.min.$p13Name + $p13Role.offsetCm[$p13Axis] -lt -$p13Half -or
            $p13Mesh.bounds.max.$p13Name + $p13Role.offsetCm[$p13Axis] -gt $p13Half) { throw 'Role exceeds authoritative plot inset.' }
    }
    if ([Math]::Abs($p13Mesh.bounds.min.z) -gt 2) { throw 'Ground pivot exceeds 2cm tolerance.' }
}
foreach ($p13File in $p13Delivery.files.PSObject.Properties) {
    $p13Path = Join-Path $p13Source $p13File.Name
    if ((Get-FileHash -LiteralPath $p13Path -Algorithm SHA256).Hash -ne $p13File.Value.sha256) { throw "Checksum mismatch: $($p13File.Name)" }
}
& (Join-Path $PSScriptRoot 'ValidateEnhancedMvpPromotion.ps1') -Prompt P13
