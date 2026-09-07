# Data-driven presentation meshes

Every `UHansaDefinitionBase` now has a typed, optional `PresentationMesh` soft reference. Buildings require an assignment through their existing validation; goods, vehicles and other definitions may leave it empty. Authors select it in the native definition Details panel. The reflected editor schema includes the reference, compatible-migration metadata, serialization and validation rules. AI patches cannot assign this approved-content field (`HansaAIAccess=Never`). Asset Registry references expose the selected mesh to dependency/impact analysis.

The building projection resolves its stable definition ID through Asset Manager and loads the definition's mesh. There are no bakery or residence mesh-path switches in rendering code. The same resolver and mesh loader are available to Blueprint/C++ consumers for all registered definition types. This change adds the common contract for items; it does not introduce new world-rendering systems for otherwise non-spatial items.

Authored meshes retain their materials, cast shadows, fit uniformly down into the existing unrotated footprint, center horizontally and rest on the placement plane. Actor yaw applies placement rotation. Construction placeholders, status markers and selection remain functional. Missing references use the existing cube placeholder. Staging/developer mesh paths are rejected by the loader and reported by definition validation. Changing a mesh clears old material overrides.

## Compatible field migration and catalog impact

`PresentationMesh` moved from the building subclass into the shared base without changing its serialized name or type. Existing building assignments survive loading, including the approved laborer residence. Its existing building hash position is preserved. Empty mesh fields on other definition types add no hash data; assigning one participates in deterministic content hashing.

The approved bakery changes the actual catalog content: its authored revision is now 2 and its mesh is the promoted asset. The independently verified 72-definition catalog hash changed from `5A9D613E1EA421AA` to `724BD5DE8DB9C292`. Johan explicitly approved both promotion/assignment and this runtime-pin change on 2026-09-06. The strict runtime pin remains enforced. Saves associated with an older catalog are subject to existing compatibility rejection and may require a new game; no silent save migration is added.

## Bakery production assignment

- Definition: `/Game/Hansa/Core/Buildings/DA_Building_Bakery`
- Mesh: `/Game/Mesh/hansa-bakery/Meshes/SM_HansaBakery`
- Materials: `/Game/Mesh/hansa-bakery/Materials` (17)
- Textures: `/Game/Mesh/hansa-bakery/Textures` (51)
- Reviewed source: `SourceArt/Generated/Buildings/HansaBakery_ImageGen_20260906_02`
- Promotion record and source mapping: `Docs/Development/Evidence/BakeryPresentation_20260906/promotion.json`

The complete 69-asset dependency closure was traversed after remapping: no staging or developer dependency remains. The high-detail mesh has Nanite enabled and one convex selection hull; the world component uses visibility-query collision and does not affect navigation. The existing bakery 3-by-2 cell gameplay footprint and economy are unchanged. Runtime fitting preserves aspect ratio, making this long source building smaller than its source architectural dimensions.

This promotion preserves the reviewed ImageGen material work: four native 1254-by-1254 source images contribute to nine base-color variants, alongside hybrid PBR roughness/normal maps. Other maps remain at their original 1024-by-1024 size. No new generation or source resampling occurred during integration. Generation prompts, material assessments and native render evidence remain in the reviewed source package. The production assets are imported game assets, not mockup references.

## Verification

`Hansa.UI.World` checks generic data assignment, mesh/material switching, uniform footprint fitting, construction state, existing laborer assignment and common good/vehicle mesh support. The bakery check explicitly requires the approved production path. `Hansa.Architecture.Authoring.EconomicSchemaCoverage` checks the shared field across all 13 definition schemas. `Hansa.Integration.Authoring.EconomicAssetReload` recompiles all 72 saved definitions and compares their catalog hash to the runtime pin.

The first post-promotion reload intentionally exposed the old pin mismatch and established the exact replacement value; it did not report other validation errors. Final test/build evidence is recorded alongside this document's promotion evidence.

Live PIE verification on `L_Lubeck_MVP` confirmed that the visible `HansaBuildingProjection_3_1.BuildingMesh` uses the approved bakery production path at uniform scale 0.3900436233; the laborer projection retains its existing approved mesh at native scale. See `runtime_meshes.json`. The generic editor viewport capture targets the edit world rather than PIE and was excluded from runtime evidence. A final in-game visual screenshot and a fully cooked distribution package were not verified in this integration pass.
