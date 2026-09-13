# Brewery production promotion

Johan explicitly approved Brewery production promotion and game-data integration on 2026-09-12. This record supersedes the earlier staging/approval-pending statements in the archived generation job.

## Runtime contract

- Definition: `/Game/Hansa/Core/Buildings/DA_Building_Brewery.DA_Building_Brewery`.
- Stable identity: `Building.Brewery`; recipe: `Recipe.BrewBeer`.
- Current binding: `/Game/Mesh/hansa-brewery-huexstrasse128/Production/SM_HansaBrewery_Production.SM_HansaBrewery_Production`.
- Authored revision 4, 4×4 parcel, native model scale. Model envelope approximately 12.24 × 15.55 × 15.58 metres.
- Catalog v14: `73EC37D013D49BA0`; immediate prior v13: `8586FD71211B707C`. Existing saves require a new game under the catalog compatibility contract.
- Brewing remains 3 malt + 1 hops + 1 empty barrel → 5 beer per 100 ticks; 4 laborers and 2 artisans. Promotion does not change production economics.
- Runtime projection selects the authored mesh without a hard-coded Brewery switch. Under-construction presentation remains unchanged.

## Source and evidence

- Editable [Blender master](Model/hansa-brewery-huexstrasse128.blend).
- [Combined Unreal FBX](Model/SM_HansaBrewery_Production_Combined.fbx), SHA-256 `8139D8201DD29C8BBCC1B9BC9A97B49202F1175686210ABFE5ACB1E1FC558E8A`.
- [Research and delivery archive](../../Generated/Buildings/HansaBrewery_Huexstrasse128_20260912/README.md), including evaluation, provenance, iteration log, photographic reference manifest, ImageGen prompts, comparisons, and turntable.
- ImageGen base-color masters retain native 1254×1254 dimensions. Independent physical maps are 1024×1024; normal textures use linear normal compression with green-channel conversion for Unreal.
- Sixteen explicitly assigned material families; conventional full-detail static-mesh rendering (Nanite disabled after thin-detail instability in review); eight-hull convex collision approximation.
- Isolated saved/reopened preview: `/Game/Hansa/Generated/Staging/BreweryPromotion_20260912/L_Brewery_ProductionReview`. No gameplay map was modified.

## Automated verification

All 19 targeted tests passed after the combined export:

- `Hansa.Simulation.Production`: 6 tests, including the deterministic 1,000-tick chain.
- `Hansa.UI.World`: 9 tests, including `BreweryProductionPresentation` (saved mesh, recipe, footprint, full scale, materials, completed visibility, construction placeholder).
- `Hansa.Integration.Authoring`: 4 tests, including saved catalog reload, order-independent hashes, and historical catalog reconstruction.
- Editor Development build passed.
- Recursive production dependency audit passed with no staging/developer dependencies. Both rejected Brewery staging folders now have explicit NeverCook entries; the isolated preview is covered by the existing generated-staging exclusion.

Artifacts: `Saved/BuildArtifacts/20260912-170610653-build-HansaEditor-Win64-Development`, `20260912-170644911-automation-Hansa.Simulation.Production`, `20260912-170659211-automation-Hansa.UI.World`, and `20260912-170712401-automation-Hansa.Integration.Authoring`.

## Visual acceptance and limits

The final conventional-mesh hero and brewing-yard close-up were inspected after a fresh saved-level reload. Roof tiles, gable/window details, and production props remain intact; the wall repeat and restrained relief remove the earlier large-scale diagonal striping. Native captures are 2825×1192 pixels, with no resampling.

- [Native Unreal hero](../../Generated/Buildings/HansaBrewery_Huexstrasse128_20260912/renders/unreal-production-hero.png).
- [Facade](../../Generated/Buildings/HansaBrewery_Huexstrasse128_20260912/renders/unreal-production-facade.png), [roof](../../Generated/Buildings/HansaBrewery_Huexstrasse128_20260912/renders/unreal-production-roof.png), and [brewing yard](../../Generated/Buildings/HansaBrewery_Huexstrasse128_20260912/renders/unreal-production-yard.png).
- Final mesh SHA-256: `BE36BE5622BB8A173FC58ECE0669B6596757B65D65153AE4F7FEA189A3519880`.
- [Fresh-process saved-asset/dependency audit](Evidence/promotion-verification.json).

The unbound diagnostic mesh was moved to `/Game/Hansa/Generated/Staging/BreweryPromotion_20260912/SM_Brewery_SectionDiagnostic`; it is outside the runtime dependency closure. Rejected staging files were retained, not deleted. The final audit also encountered an occupied local tool-server port while another editor was open; its Python verification still passed.

Runtime/data acceptance is verified. Engine review corrections and remaining visual approximations are recorded in [the material assessment](MATERIAL_ASSESSMENT.md). The section audit ruled out incorrect final material indices. Full-detail static-mesh rendering replaces the unstable reduced/cold Nanite views; source roof tint layers and restrained masonry relief were restored explicitly.

Rear-yard brewing details are inferred, interiors/windows are simplified for strategy-camera use, and collision is coarse. Non-power-of-two base-color maps still require platform mip/streaming review. A complete Shipping cook, performance budget, and multi-resolution gameplay UAT were not performed. The audit commandlet's script passed, but its process returned 1 because the project has an existing GameFeatureData AssetManager startup error; this is not reported as a clean commandlet exit.
