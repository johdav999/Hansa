# Compact labour courts — R07, 2026-09-17

User request: keep the same buildings on about two-thirds of the land and add a few lean-tos.

Implemented four existing compound families, three development stages, two persistent alternatives and four road contexts (96 layouts). Existing dwelling, workshop, storage and utility meshes remain at exact unit scale. No raster image, mesh, texture, material or provider call was needed.

The four-metre placement grid and final-stage door clearance yield a 25% reduction in occupied area: NarrowGang changes from 16 x 12 m to 12 x 12 m; SharedCourt, CornerCourt and CraftCourt change from 16 x 16 m to 16 x 12 m. This is 75% of the old area, rather than exactly two-thirds. It raises residences per area by one-third. All twelve owning building-stage footprints match their compound, so placement, road connection, previews and upgrades use the smaller parcels.

Buildings are repositioned and, where necessary, turned to preserve accessible doors. Three continuous fence sides are reconstructed from the approved two-metre modules; the street front remains open. Required household cargo and optional small props occupy clear spaces by the structures. Existing approved weighted 25-degree alternatives remain on eligible first-stage houses. Later crowded stages use aligned alternatives where additional turns would obstruct access.

One approved `SM_LabourKit_LeanTo` is added to each early-stage layout (64 stage/context records). Final-stage plots retain all their original buildings and omit the additional lean-to where its complete envelope would obstruct circulation. The same stable layout/slot IDs and parcel seed determine variation. Natural terrain fitting and worn entrance paths remain native.

## Files and compatibility

- Reproducible geometry authoring: `SourceArt/Generated/Compounds/LabourCourts_20260915/R07/compact_courts.py`.
- New definitions: sibling `definitions/` directory; complete native pre-edit compound exports in `baseline/`.
- Production compounds: `/Game/Hansa/Core/Compounds/LabourCourts/DA_Compound_<family>`.
- Matching stage bindings: `/Game/Hansa/Core/Buildings/LabourCourts/`.
- Reused lean-to: `/Game/Mesh/labour-housing-kit/Meshes_R08/SM_LabourKit_LeanTo`.
- New reviewed manifest: `Tests/Golden/economic_catalog_v27.json`, hash `BD2FC9656111389A`; previous v26 remains `0ECFB6BA46CD1344`.

The existing schema and generic Details/interchange contract are unchanged. The compaction commandlet validates all candidate layouts and matching transient building bindings before writes, only allows footprint reductions with explicit `-Compact`, preserves localization identity and exports the exact previous compound definitions. The economic lineage test reconstructs every v26 fingerprint before testing older catalogues. Save files are untouched: the existing exact-catalogue contract requires a new game rather than silently resizing occupied plots in old saves. Costs, residents, production and household needs are unchanged.

## Verification

- Local geometry checks: all 96 layouts, every possible structural variant, doorway, pedestrian node and path link pass.
- Native draft validation: 96 layouts and 6,144 seeded compositions, forward/reverse catalogue compilation; exactly four compound and twelve dependent building fingerprints changed.
- Native application: result 0, 16 changes, hash `BD2FC9656111389A`.
- Editor Development build passed. Independent native disk reload: changed=0, result 0, exact hash BD2FC9656111389A.
- All 18 selected non-rendered tests passed, including CompactCourts, ContinuousBoundaries, DecorationVariation, RandomLabourConstruction, EconomicAssetReload (full historical catalogue reconstruction) and RuntimeAuthorityContinuation. Report: Saved/Automation/CompactCourts.
- The rendered fixture was updated to derive road adjacency and district alignment from each definition footprint; the construction tooltip now states the compact dimensions. Rendered results follow below.

No new media or runtime/editor dependency was introduced. This content revision does not claim a fresh Shipping cook.

### Final rendered verification

Final Development Editor rebuild passed after one transient MSVC C1001 retry. `Hansa.Compound.PlayerFlow` passed on the native 1920 x 1080 viewport: normal placement and rotation, construction, two upgrades, exact save/load reconstruction, demolition/replacement and six adjacent randomly selected courts. Log: `Saved/Logs/CompactCourtsPlayerFlowFinal.log`.

The [native close view](CompactLabourCourts/close.png) was inspected: houses remain full-size with intact materials, continuous boundaries, visible lean-to shelters and unobstructed entrances. The [district](CompactLabourCourts/district.png) and [final-stage](CompactLabourCourts/stage3.png) captures are also preserved without resizing. This automated map has simplified surrounding terrain and buildings; it is not a review of the final Lübeck landscape. Total targeted tests: 18 non-rendered plus the rendered player flow, all passing.

The live property tool rejected simultaneous nested array replacement before saving. The validated native commandlet performed the final application with the editor closed; independent disk reload proved the exact candidate. `R07/applied.json` is the authoritative application receipt. Existing GameFeatureData configuration warnings appeared during commandlet startup; validation returned result 0.
