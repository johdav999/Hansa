param([string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
function Require($Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function NativePngSize([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    Require ($bytes.Length -gt 24 -and $bytes[0] -eq 137 -and $bytes[1] -eq 80) "Invalid PNG: $Path"
    $w = [byte[]]$bytes[16..19]; $h = [byte[]]$bytes[20..23]
    [Array]::Reverse($w); [Array]::Reverse($h)
    return @([BitConverter]::ToUInt32($w,0), [BitConverter]::ToUInt32($h,0))
}
$contract = Get-Content -Raw (Join-Path $ProjectRoot 'Tests/Golden/enhanced_mvp_mill_asset_v1.json') | ConvertFrom-Json
$source = Join-Path $ProjectRoot $contract.sourceRoot
$engine = Get-Content -Raw (Join-Path $source 'evidence/unreal_reopen.json') | ConvertFrom-Json
$sweep = Get-Content -Raw (Join-Path $source 'evidence/clearance.json') | ConvertFrom-Json
Require $engine.savedAndReopened 'Saved/reopened engine proof is missing'
Require ($engine.blueprint -eq $contract.blueprint) 'Production Blueprint changed unexpectedly'
Require (($engine.nativeScale -join ',') -eq '1,1,1') 'Runtime auto-fit is forbidden'
for ($i=0; $i -lt 3; $i++) { Require ([Math]::Abs($engine.pivotCm[$i]-$contract.rotorPivotCm[$i]) -lt .1) 'Incorrect rotor pivot' }
foreach ($part in @('Body','Rotor')) {
    $mesh = $engine.meshes.$part
    $name = 'SM_Mill_' + $part
    Require ($mesh.path -eq ($contract.assetRoot+$name+'.'+$name)) "Wrong $part asset"
    Require (Test-Path -LiteralPath (Join-Path $ProjectRoot ('Content/Mesh/hansa-mill/P09/Meshes/'+$name+'.uasset'))) "Missing $part package"
    Require ($mesh.lodTriangles.Count -eq 3 -and $mesh.lodTriangles[0] -gt $mesh.lodTriangles[1] -and $mesh.lodTriangles[1] -gt $mesh.lodTriangles[2] -and $mesh.lodTriangles[2] -gt 0) "Invalid $part LOD chain"
    Require ($mesh.vertexColorImportOption -match 'REPLACE') "Weathering import disabled on $part"
    for ($i=0; $i -lt 3; $i++) { Require ([Math]::Abs($mesh.lodThresholds[$i]-$contract.lodThresholds[$i]) -lt .001) "Wrong $part LOD thresholds" }
    foreach ($material in $mesh.materials) { Require ($material.StartsWith('/Game/Hansa/Core/Buildings/Windmill/Materials/')) "Unexpected material $material" }
}
$body = $engine.meshes.Body
Require ($body.convexCollisionCount -gt 0) 'Body needs simple convex collision'
Require ($engine.meshes.Rotor.collisionCount -eq 0 -and $engine.meshes.Rotor.convexCollisionCount -eq 0) 'Rotor must have no collision'
Require ([Math]::Abs($body.boundsCm[0][2]) -le 2) 'Ground pivot exceeds tolerance'
for ($i=0; $i -lt 2; $i++) { Require (($body.boundsCm[1][$i]-$body.boundsCm[0][$i]) -le 1160) 'Grounded body does not fit the authored plot' }
Require ($sweep.passed -and $sweep.sweepSamples.Count -eq 72 -and $sweep.minimumSailHeightM -gt 2.5) 'Sail clearance failed'
foreach ($sample in $sweep.sweepSamples) { Require ($sample.overlaps -eq 0) 'Sails intersect the body' }
foreach ($path in $engine.productionDependencyClosure) { Require ($path -notmatch '/Staging/|/Developer|/Engine/BasicShapes/') "Non-production dependency: $path" }
foreach ($format in @('fbx','glb')) {
    $check = Get-Content -Raw (Join-Path $source "evidence/reimport_$format.json") | ConvertFrom-Json
    Require $check.uv0 "Missing UV0 in $format"
    foreach ($image in $check.packedMasterImages) { Require $image.packed "Unpacked source image: $($image.name)" }
}
$maps = @(Get-ChildItem -LiteralPath (Join-Path $source 'exports') -Filter 'T_Mill_*.png')
Require ($maps.Count -eq 24) 'Expected 24 native PBR maps'
foreach ($map in $maps) { Require (((NativePngSize $map.FullName) -join ',') -eq '1024,1024') "Wrong native PBR size: $map" }
foreach ($map in Get-ChildItem -LiteralPath (Join-Path $source 'textures') -Filter '*.png') {
    Require (((NativePngSize $map.FullName) -join ',') -eq '1254,1254') 'ImageGen source size changed'
    Require (Test-Path -LiteralPath ([IO.Path]::ChangeExtension($map.FullName,'prompt.md'))) 'Missing original prompt'
}
foreach ($name in @('neutral_25m','neutral_65m','neutral_120m','warm_25m','warm_65m','warm_120m','roof_detail','base_detail')) {
    $size = NativePngSize (Join-Path $source "evidence/unreal_$name.png")
    Require ($size[0] -gt 600 -and $size[1] -gt 400) "Missing native engine capture: $name"
}
$tests = Get-Content -Raw (Join-Path $source 'evidence/validation.json') | ConvertFrom-Json
foreach ($name in $contract.requiredTests) { Require (@($tests.tests | Where-Object { $_.name -eq $name -and $_.state -eq 'Success' }).Count -eq 1) "Required regression did not pass: $name" }
$previous = Join-Path $ProjectRoot 'SourceArt/Generated/Buildings/HansaWindmillAnimated_20260906_04/exports/HansaWindmill_Animated.blend'
Require ((Get-FileHash -LiteralPath $previous -Algorithm SHA256).Hash -eq $contract.sourceMasterPreviousSha256) 'Previous approved master changed'
Write-Output 'PASS EMVP-P09: authored scale, pivot, LODs, materials, swept clearance, production references, native evidence and regression tests.'
