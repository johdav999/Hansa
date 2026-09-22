# Preserved fish in the main project

Implemented following the user request to integrate into main. Open the normal `Hansa.uproject`, restart the editor to load rebuilt modules, and start New Game. Select a completed fishing hut, choose **Build salting shed**, wait for construction, then choose **Salted catch**. Supply salt and barrels; four laborers are needed. Optional **Fresh fallback** keeps producing fresh fish when preservation inputs are unavailable. The household need remains **Fish**.

## Integration

Reviewed three-way merge of 65 changed/new source files against preserved snapshots retained the existing heating controls, seasonal demand, reserve policy, save fields and firewood glyphs. Overlapping population arithmetic uses checked fixed-point operations. The original v24 Core assets were backed up before the ten preservation asset updates/additions. The verified 3D mesh/materials were promoted to `/Game/Mesh/hansa-fish-preservation/` and are bound to Building.Fishery.SaltingShed. Existing original fishery identity and mesh remain available.

Accepted preservation catalog v25 is D73BFD73C23C2D03 (102 definitions). The older staged Firewood/P33 selectors skip preservation's new IDs to preserve their exact reviewed catalogs. The separately approved firewood promotion is sequenced afterward as additive v26; it must retain preservation. That task owns the next catalog/seed/pin update. This report records the completed v25 handover rather than claiming v25 must remain the latest catalog.

## Verification

- Main HansaEditor Win64 Development build passed (`build-2.log`). The first attempt was blocked only by another editor's loaded DLLs; the other task closed its own session before the successful build.
- 29 main-project tests passed: preservation simulation/validation/impact, modest-stock paid trade, full catalog lineage, firewood policy/workshop/calendar/reserve compatibility and save/restore coverage.
- Native `Hansa.UI.PreservedFish.RealFlow` passed at both 1280 x 720 and 1920 x 1080 on the configured Lübeck map, with ordinary New Game and no candidate catalog flag. Captures were visually inspected at original dimensions, including large text/high contrast: the upgrade, selected/active mode, fallback and missing-input messages remain readable.
- Evidence: `Saved/GenerationJobs/preservedfish_main_20260917/integration-result.json`; GUI images and semantic TSVs: `Docs/Images/UI/PreservedFish/MainProject/`.
- Model dimensions, LOD/collision/material audit, ImageGen icon provenance and prior isolated Shipping smoke evidence remain in `PreservedFish.md` and the model source directory. No new imagery or resampling was needed for main promotion.

## Compatibility and limits

Save format9/fingerprint22/command7; existing saves from an incompatible catalog are rejected explicitly rather than silently reinterpreted. Keep those saves; start a new game for the new catalog. This main integration verification covers the requested fishing-hut GUI flow and regressions. A full household/trade campaign and release-wide performance/packaging acceptance remain broader gates. The main project retains its pre-existing configured terrain-preview map; this integration does not replace the user's map.

## Combined catalog verification (2026-09-17)

Main now uses combined catalog v26 (`0ECFB6BA46CD1344`), retaining preserved fish and approved firewood. Rechecked the existing implementation without overwriting either feature: all eight targeted preservation regression tests passed, and `Hansa.UI.PreservedFish.RealFlow` passed at 1280 x 720 on the configured main map with ordinary New Game. The GUI test paid for the salting-shed upgrade, completed construction, selected Salted catch, and enabled Fresh fallback. Evidence is in `Saved/GenerationJobs/preservedfish_main_20260917/v26-verification.json` and the sibling `main-v26-*.log` files; captures and semantic snapshots are under `Docs/Images/UI/PreservedFish/MainProject/v26/`. Existing engine toolset Python startup errors and the dirt-road material usage warning remain outside this feature; they did not fail the tests. No new assets or visual changes were introduced in this recheck.
