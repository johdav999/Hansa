[CmdletBinding()]
param([Parameter(Mandatory=$true)][ValidateSet('P12','P13','P14','P15','P17','P18')][string]$Prompt)
$ErrorActionPreference = 'Stop'
$promotionRepo = Split-Path $PSScriptRoot -Parent
$promotionReceipt = Get-Content -Raw (Join-Path $promotionRepo "Docs/Development/Evidence/${Prompt}ApprovedPromotion-20260908.json") | ConvertFrom-Json
$expectedCatalog = if ($Prompt -eq 'P18') { 7 } elseif ($Prompt -eq 'P17') { 6 } else { 5 }
if ($promotionReceipt.prompt -ne $Prompt -or $promotionReceipt.catalogVersion -ne $expectedCatalog -or !$promotionReceipt.economicsUnchanged) { throw 'Invalid approval receipt.' }
foreach ($asset in $promotionReceipt.assets) {
    if (!$asset.destination.StartsWith($promotionReceipt.productionRoot + '/')) { throw 'Asset escapes approved destination.' }
    $package = Join-Path $promotionRepo ($asset.destination.Replace('/Game/', 'Content/') + '.uasset')
    if ((Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash -ne $asset.sha256) { throw "Promoted package changed: $package" }
    $source = Join-Path $promotionRepo ($asset.source.Replace('/Game/', 'Content/') + '.uasset')
    if ((Test-Path -LiteralPath $source) -and (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $asset.sourceSha256) { throw "Staged source changed: $source" }
}
foreach ($definition in $promotionReceipt.definitions) {
    $package = Join-Path $promotionRepo ($definition.path.Replace('/Game/', 'Content/') + '.uasset')
    if ((Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash -ne $definition.sha256) { throw "Bound definition changed: $package" }
}
Write-Output "$Prompt approval receipt and saved package hashes pass. Live gameplay and Shipping acceptance are separate gates."
