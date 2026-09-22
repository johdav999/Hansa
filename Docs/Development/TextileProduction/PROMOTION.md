# Approved textile production promotion — 2026-09-19

User approval: “promote these”, following the tested TextileProductionV2 playable review.

Accepted catalog **v30**, registry hash **3472D1CD822731FA**, **181 definitions**. The exact hash was reproduced after saving and in a fresh Unreal process. `Tests/Golden/economic_catalog_v30.json` records the per-definition hashes. Previous fixtures remain preserved.

## Scope

- Seven goods, five recipes, four Craftsmen workshops and two household needs promoted from the reviewed snapshot.
- The existing v29 regional economy was retained. Textile market entries and Craftsmen needs were added to its current definitions, instead of replacing the catalog with the older candidate snapshot.
- Meshes, materials and textures copied to `/Game/Mesh/hansa-textile-production/`. Persisted dependency verification found no references from these assets to generated staging.
- Definitions live under `/Game/Hansa/Core/`. The eleven approved ImageGen asset families remain in `Content/Hansa/UI/TextileProduction/`, now packaged for Shipping as well as Development. Source masters and prompt records remain unchanged.
- The normal runtime loads v30 without a candidate flag. The preview script now defaults to the accepted catalog; `-Candidate` explicitly selects the preserved staging review.
- 97 packages saved. Backups of overwritten accepted definitions are under `Saved/GenerationJobs/TextileProductionV2/promotion-baseline/`. Both staging snapshots remain available.

## Save compatibility

Start a **New Game** for the expanded economy. Older saves are preserved but rejected by the existing catalog compatibility guard; there is no silent migration or hash bypass. The immediate previous catalog is v29 (`0698066FA49B59A2`).

## Verification

- Fresh accepted reload and model staging-dependency check: `Saved/Logs/TextilePromotedDependencies.log`.
- Accepted catalog fixture/reload test passed: `Saved/BuildArtifacts/20260919-184545035-automation-Hansa.Integration.Authoring.EconomicAssetReload`.
- All five textile mechanics tests passed: `Saved/BuildArtifacts/20260919-184602246-automation-Hansa.TextileProduction`.

The previous manual-controller, staffed-economy playthrough and clean cooked-package gates remain release checks; the user's promotion approval is fulfilled. Earlier review evidence remains in `UAT/SESSION.md`.
Final checks:
- Rendered normal-game review passed without `-TextileProductionCandidate`: `Saved/BuildArtifacts/20260919-184619022-textile-playable-review`. All four workshop production mesh bindings, native recipe actions and save/load were checked.
- Final Development runtime build passed: `Saved/BuildArtifacts/20260919-184616005-build-Hansa-Win64-Development`.
- Final Shipping build and binary/receipt audit passed: `Saved/BuildArtifacts/20260919-184642313-shipping-exclusion-Win64/result.json`.
- Approved-catalog screenshots are preserved in `UAT/promoted/`.
