# Labour court decoration and occasional house yaw — 2026-09-15

Requested: enrich existing courts with props/fences and occasional slight house yaw irregularity.

## Implemented content

- All four production families, all three stages and all four road contexts (96 layouts).
- Existing approved 2 m timber fence modules, including incomplete boundaries from stage one. Street frontage, corner openings and the existing access graph remain clear.
- Four authored household-prop slots per layout: two market cargo clusters and two baskets at their existing real scale. One cargo cluster is always present; the other three slots have 75% presence. These are decorative inventory, not simulated goods.
- Dwelling slots with safe clearance gain weighted explicit -3/+3 degree alternatives relative to their original alignment. Original weight 8, each safe alternative weight 1: 80–89% retain their original alignment. Tight slots remain unchanged. The same parcel/slot seed controls inclusion and yaw, with no per-frame randomization or architectural rescaling.
- 108 dwelling slots gained safe yaw choices; 494 fence slots and 384 prop slots were added across the 96 layouts. Counts include each stage/context record.
- Existing terrain-fit, instancing, foundations and preview/runtime composition paths are reused. No new mesh, texture, provider call, or GUI artwork.

## Source and production paths

- Reproducible revision: `SourceArt/Generated/Compounds/LabourCourts_20260915/R04/revise_courts.py`.
- Revised source: `R04/definitions/<family>.json`; original R03 sources remain intact.
- Native pre-edit definition exports: `R04/baseline/`.
- Exact editor property readback and application receipt: `R04/*-editor-after.json`, `R04/applied.json`.
- Production: `/Game/Hansa/Core/Compounds/LabourCourts/DA_Compound_<family>`.
- Reused props: `/Game/Mesh/hansa-market/Meshes/SM_HansaMarket_Cargo` and `SM_HansaMarket_Basket`.
- Reused fences: `/Game/Mesh/labour-housing-kit/Meshes_R08/SM_LabourKit_Fence2m`.

## Authoring and validation

The existing reflected variant weight/yaw and optional-slot fields remain authoritative. No schema or serialization change. Existing validators check every possible envelope, unit scale, parcel containment, doorway approach, node clearance and path link. Editor Details, interchange and runtime consume these same fields.

`HansaCourtDecoration` imports and validates all candidate layouts before producing a complete candidate catalogue. It preserves production localization identity and compiles duplicate dependent building bindings against the candidate compounds. Forward/reversed discovery must agree. The optional commandlet apply path requires assets not held open by another editor; the actual application used the live editor's transactional property and save APIs, appending to exact native readback without clearing arrays.

The update changes four compound definition fingerprints and, transitively, the twelve building-stage fingerprints that include their compound hash. Gameplay costs, capacity, footprint, stable identity and upgrade links are unchanged.

Catalogue v20: `4A86F28719E21627`, with full manifest `Tests/Golden/economic_catalog_v20.json`. Exact v19 reconstruction uses preserved definitions and duplicated bindings against the old compounds. The game's existing exact-hash save contract remains: v19 saves need a new game with v20; no save is deleted or silently converted. Restart with a rebuilt runtime to load the new catalogue.

## Verification

- Independent Unreal validation accepts all 96 layouts and 6,144 seeded composition calls.
- Saved native assets match the validated candidate exactly (`COURT_DECORATION_VALIDATED changed=0`), with successful forward/reverse compilation.
- Editor build passed.
- Automated content, occasional yaw, catalogue lineage and native player-flow evidence: see final verification notes below.
### Final verification (2026-09-15)

- All 12 selected compound/content tests and EconomicAssetReload pass. RandomLabourConstruction was rerun successfully after synchronizing the isolated project's stale AssetManager configuration with the main project.
- Rendered Hansa.Compound.PlayerFlow passes: construction, all three stages, save/load, replacement and a six-court district. Native 1920x1080 close view inspected: fences and small household props render at the expected scale with open court access. The view uses the existing automated test map and its simplified surroundings.
- Fixed and saved M_Market_Sack's InstancedStaticMeshes usage after the rendered check exposed a fallback warning for cargo sacks.
- Win64 Shipping build and receipt/executable exclusion audit pass. This is not a cooked-package audit.
- Main editor was left running; its loaded runtime binaries must be rebuilt/restarted before playing catalogue v20. The isolated verification editor used the new code and saved production assets.

Final post-material-fix PlayerFlow rerun: Success, exit 0, no missing-instancing-usage warnings. Final native screenshot inspected and preserved as [close.png](LabourCourtDecoration/close.png). Existing engine ToolsetRegistry Python startup errors remain in the game-mode log and did not fail the gameplay test.

## R05 — continuous boundary fences and larger turns (2026-09-16)

Supersedes the R04 fence and yaw treatment above at the user's request. All 96 layouts now have required, continuous fence runs on local -X and both Y sides. Local +X is the street frontage and stays open; the whole parcel rotates to face its actual road. Corner contexts retain these same three enclosed sides. Existing two-metre modules stay at unit scale and meet at shared end posts. Only thin fence ends/corners may overlap by up to 25cm; stacked sections and mid-panel crossings remain invalid. All structure, access and footprint checks remain active.

Occasional house alternatives now turn exactly -25 or +25 degrees relative to their original alignment. Weight 8 retains the original, weight 1 selects each safe turn. Small authored translations of up to 150cm per axis give the rotated house clearance where possible. There are 72 eligible slots across the pack. NarrowGang has no safe 25-degree dwelling option within its existing 16x12m footprint and central access corridor, so its houses remain aligned. The more spacious families provide the larger turns; cramped stage-three CornerCourt houses also stay aligned. Props remain present.

R05 source and native pre-edit exports are in `SourceArt/Generated/Compounds/LabourCourts_20260915/R05/`. No new mesh or raster was generated. The live editor connection closed during a property transaction before saving; production changes were then applied successfully through the native HansaCourtDecoration commandlet with the editor closed. Its original baseline exports preserve v20.

Catalogue v21: `5E0327141B0AC574`, previous v20 `4A86F28719E21627`. Only the four compound and twelve dependent building fingerprints change. The existing exact-hash save policy requires a new game; no save files are changed. Generic Details and JSON interchange retain the same schema. Metadata now documents the 25-degree principal limit and fence end-joint validation. The changed validator also applies to preview and runtime composition.

### R05 final verification

- Native commandlet validates all 96 layouts and 6,144 composition calls, then saves all four production definitions successfully.
- Main and isolated HansaEditor Development builds pass.
- All 14 selected compound and economic-asset regression tests pass, including continuous boundary sampling, rejection of stacked fence sections, occasional rotation, access/overlap validation, runtime construction, and exact v20/v19 catalogue reconstruction.
- Rendered PlayerFlow passes (exit 0), exercising construction, upgrades, save/load and replacement. Native 1920x1080 [final close view](LabourCourtDecoration/R05/close.png) inspected: three continuous sides and open street frontage, intact house/prop materials. Existing simplified test-map surroundings remain. The run has an unrelated existing road-material SplineMeshes usage warning.
- Win64 Shipping build and receipt/executable exclusion audit pass; no cooked-package audit is claimed.
- Main editor binaries are rebuilt on disk. Reopen the editor and start a new game to use v21; old v20 saves are unchanged and remain incompatible with the new exact catalogue hash.

## R06 — 80% rotation frequency (2026-09-16)

User requested 20% original, 40% -25°, 40% +25° for the rear house and CornerCourt/CraftCourt main house. R06 changes 44 existing safe three-variant slot records to weights 1/2/2. Tight slots without approved turns remain aligned. No geometric changes. Source, native before/after readback and validation are in `SourceArt/Generated/Compounds/LabourCourts_20260915/R06/`. Catalogue v23 is `A2B48E339BEE2EFA`; v22 saves retain the existing new-game requirement.
