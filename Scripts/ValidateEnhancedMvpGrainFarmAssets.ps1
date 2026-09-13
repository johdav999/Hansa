[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$ManifestPath = Join-Path $Root 'Tests/Golden/enhanced_mvp_grain_farm_asset_family_v1.json'
$Manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json

if ($Manifest.deliveryPrompt -ne 'EMVP-P08' -or $Manifest.definitionId -ne 'Building.GrainFarm') {
    throw 'P08 grain-farm manifest identity is invalid.'
}
if ($Manifest.gridStepCm -ne 400 -or $Manifest.footprintCells[0] -ne 4 -or $Manifest.footprintCells[1] -ne 4) {
    throw 'P08 grid or footprint contract drifted.'
}

$ExpectedRoles = @('completed-building','construction','working-field-center','working-field-edge','working-field-corner','idle-field-furrow','working-props')
foreach ($Role in $ExpectedRoles) {
    $Asset = $Manifest.assets | Where-Object role -eq $Role
    if ($null -eq $Asset) { throw "Missing P08 role: $Role" }
    if ($Asset.lodCount -lt 3) { throw "P08 role lacks three LODs: $Role" }
    if (($Asset.runtimeScale -join ',') -ne '1,1,1') { throw "P08 role is not identity scale: $Role" }
    $Relative = ($Asset.path -replace '^/Game/', 'Content/' -replace '/', '\\') + '.uasset'
    if (-not (Test-Path -LiteralPath (Join-Path $Root $Relative))) { throw "Missing promoted Unreal asset for $Role`: $Relative" }
}

$Building = $Manifest.assets | Where-Object role -eq 'completed-building'
if ($Manifest.assemblyBoundsCm[0] -gt 1560 -or $Manifest.assemblyBoundsCm[1] -gt 1560) { throw 'P08 whole assembly exceeds the inset.' }
if ($Manifest.presentationActorClass -ne '/Script/Hansa.HansaGrainFarmPresentation') { throw 'P08 state presentation is not bound.' }
if ($Building.boundsCm[0] -gt $Manifest.insetAvailableCm[0] -or $Building.boundsCm[1] -gt $Manifest.insetAvailableCm[1]) {
    throw 'P08 completed building exceeds the authored 4x4 inset.'
}
foreach ($Field in $Manifest.assets | Where-Object role -like '*field*') {
    if ($Field.gridBodyCm -and ($Field.gridBodyCm[0] -ne 400 -or $Field.gridBodyCm[1] -ne 400)) { throw "Field module is not a native 4m tile: $($Field.role)" }
}

Write-Host "EMVP-P08 Grain Farm asset manifest validated ($($Manifest.assets.Count) promoted assets)."
