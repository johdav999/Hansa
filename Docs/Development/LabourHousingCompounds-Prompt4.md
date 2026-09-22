# Labour compounds: settlement integration and UAT

Production integration approved by the user (“Yes i approve”) on 2026-09-15 and executed. The four reviewed R03 definitions now reside under `/Game/Hansa/Core/Compounds/LabourCourts`; twelve new building bindings reside under `/Game/Hansa/Core/Buildings/LabourCourts`. Source hashes and complete reflected layout readback were checked against the reviewed definitions. See `SourceArt/Generated/Compounds/LabourCourts_20260915/promotion-executed.json`. The earlier automatic-review rejection is resolved by this explicit approval.

## Issue ledger

| ID | Priority | Finding | Acceptance | Status |
|---|---|---|---|---|
| LC-01 | P1 | Reviewed compounds unavailable in construction catalogue | Four stage-one cards and twelve production bindings; unchanged legacy IDs/footprints | Promoted and validated; four stage-one cards exercised |
| LC-02 | P1 | Upgrade gateway only permits tier changes | Same compound/district/tier, consecutive stage, same footprint; costs and satisfaction enforced | Implemented; verification below |
| LC-03 | P2 | Rehashes all layout data on every projection refresh | Use definition hash refreshed on load/edit/save; rebuild on changed content | Implemented; verification below |
| LC-07 | P2 | Placement ghost always used straight-road context | Rotated corner ghost matches placed context | Verified by compound regression and native corner district |
| LC-04 | P1 | Real world placement/save/load and ground contact unverified | Native viewport, semantic intents, matched composition/identity, access graph and demolition | Native PlayerFlow passed at both reference resolutions |
| LC-06 | P1 | Real game substitutes default material: five kit materials lack instanced usage | Explicit HISM shader usage saved; same native district shows approved PBR materials with no fallback warnings | Implemented; verification below |
| LC-05 | P2 | Dense district rendering costs unmeasured | Actor/instance/batch counts, draw calls, frame times and memory at fixed camera | Both preview and authoritative settlement measured |

| LC-08 | P1 | New catalog rejected by old registry pin | Reviewed manifest, preserved baseline and deterministic discovery | Catalog v19, reload lineage test passed |
| LC-09 | P2 | Upgrade stages clutter Homes catalogue | Only four new stage-one cards appear | Native catalogue verified |
| LC-10 | P1 | Residence cohort prevents compound demolition | Remove own cohort atomically while retaining cargo/inventory guards | Native demolition/replacement passed; save-checksum regression added |
| LC-11 | P2 | Inspector shows previous stage or stable-ID leaf | Resolve current authored title from authoritative record | Immediate title assertion and native screenshot passed |

## Profile and flows

Unreal city builder, mouse/keyboard/controller, production New Game on configured surveyed Lubeck terrain. Existing Slate component system, icons, inspector and save service. No new UI raster assets. Cohort population is authoritative; pedestrian nodes are presentation access metadata, not individually simulated residents.

1. Frontend -> residences catalogue -> road -> rotate/place -> construction.
2. World selection -> population/needs -> stage 2 -> stage 3.
3. Save -> load -> identical building ID, occupied cells and composition.
4. Confirm demolition -> replace -> new entity identity.
5. Adjacent straight/corner parcels at gameplay, close and distant cameras.

Performance authority: TechnicalArchitecture section 17 requires HISM, no per-citizen replicated Actors and dirty-driven presentation. It provides workload goals, but no numeric GPU/frame/memory thresholds. MVP section 12 excludes full-campaign load targets. Report measurements without invented pass thresholds.

## Implemented changes

- Runtime and editor permit consecutive development stages of the same compound, district, population tier and footprint. Costs, completed construction and satisfaction requirements still apply. Skipping a stage, changing districts/compound/footprint, or replaying invalid progression is rejected atomically.
- Existing 8 × 8 m laborer/artisan definitions and their tier progression remain unchanged. New identities reserve 16 × 12 m or 16 × 16 m at stage one.
- `HansaLabourCourtBindings` commandlet validates all twelve bindings together and refuses overwrites. Default mode is dry-run; `-Apply` writes only new building assets after reviewed compounds exist in the approved production path. Four stage-one cards reuse the native Homes catalogue; stages two and three are upgrade-only. All stages preserve the existing 12-resident parcel capacity and explicit source construction costs. Cosmetic child meshes do not multiply population or workshop output.
- Runtime registry loading includes compounds and permits these additions while retaining the 81-definition baseline completeness guard.
- Inspector upgrade labels use authored display names and correctly describe development stages as well as tier changes.
- Compound projection uses the definition hash refreshed by load/edit/save rather than reserializing all layouts on every projection refresh.
- Corner previews use adjacent road context through the build presenter.
- All six existing housing material assets explicitly retain instanced-static-mesh shader usage. The actual before capture showed default materials; the after capture shows the original lime, oak, clay, stone and iron materials. No texture, geometry, material graph or raster image was redesigned. Unreal MCP performed usage edits, recompilation, saves and flag readback; `SourceArt/Generated/Compounds/LabourCourts_20260915/hism-material-fix.json` records before/after package hashes. Historical prompt-3 preservation checks predate this intentional technical change.

## Evidence and acceptance limits

The existing production frontend -> New Game -> residence construction -> selection -> needs inspector baseline passed at native 1920 × 1080 in `Hansa.UI.ResidenceInspector.RealViewport`. [Baseline capture](LabourHousingCompounds-Prompt4/Evidence/baseline-legacy-residence-1920x1080.png).

`Hansa.Compound.PlayerFlow` operates the existing build presenter, inspector intents, construction gateway, persistent selection manager and normal save service using isolated automation slots. The real frontend-to-game sequence checks four stage-one cards, four rotations, construction, population/needs, both upgrades, exact mesh transforms and identity after save/load, confirmed demolition and replacement with a new identity. It then builds six adjacent authoritative parcels with all four families, opposing road fronts and a corner context. The 1920 × 1080 final replay passed, including immediate authored inspector titles after both upgrades. No player save is overwritten. Captures and semantic snapshots are retained in the evidence directory.

The preview benchmark enters through the real frontend and spawns 32 isolated stage-three presentation Actors on the game terrain. It does not create authoritative building records, promote staging assets, or establish populated neighborhoods. Its screenshots qualify for material/rendering inspection only. [Before fix](LabourHousingCompounds-Prompt4/Evidence/district-before-material-fix-1920x1080.png), [after fix](LabourHousingCompounds-Prompt4/Evidence/district-after-material-fix-1920x1080.png). Shadows and existing terrain are held constant. The native view includes all four families and corner layouts, with foreground parcels partly outside the viewport.

The authored 60 cm-clearance entrance/activity graph is deterministically validated; the cohort simulation does not implement individual walking residents. Native inspected parcels show grounded full-sized structures, separated footprints and developed density. Gold selection feedback follows the complete logical parcel. The capture harness initially set only transient Actor selection; it now uses the actual persistent selection manager before notifying the HUD. Close and district views preserve the approved R03 assets. This is evidence for the exercised placements, not exhaustive terrain testing. The configured game map remains a pre-existing staged terrain map; this task has not promoted it.

The placement ghost chooses corner context from adjacent roads, but its temporary anchor-derived alternative can differ from the final entity-derived alternative on confirmation. Edge-specific ghost context is not implemented. Placed layouts, upgrade seeds and loaded transforms remain deterministic. These preview limitations are recorded rather than represented as exact ghost-to-final identity parity.

Catalog v19 contains 97 definitions and pins registry hash `31FB425080110FD0`; all 81 v18 definition fingerprints are preserved. Adding new definitions changes the accepted registry hash. Existing save files and legacy footprints are untouched, but this does not establish compatibility with saves made under the old complete registry hash; existing strict mismatch handling remains. No silent footprint or save expansion is performed.

## Rendering measurement

Development `UnrealEditor -game`, D3D12, NVIDIA GeForce RTX 5060 Ti (16 GB), native 1920 × 1080, fixed player camera at maximum normal zoom. 180 warm-up frames then 180 samples per phase, simulation paused. Final measurement ran without another task-owned build/test process. The first material-fallback run overlapped headless regression work and is visual diagnosis only, not a timing comparison.

| Metric | Empty scene median | 32 preview parcels median | Difference |
|---|---:|---:|---:|
| World Actors | 64 | 96 | +32 |
| Compound Actors | 0 | 32 | +32 |
| HISM instances | 0 | 721 | +721 |
| HISM batches | 0 | 208 | +208 |
| Draw calls, complete scene | 161 | 2,500 | +2,339 |
| Observed frame interval | 5.696 ms | 6.924 ms | +1.228 ms |
| Game thread | 1.464 ms | 1.619 ms | +0.155 ms |
| Render thread | 5.677 ms | 6.909 ms | +1.232 ms |
| Process resident memory | 4,499,666,944 B | 4,611,764,224 B | +112,097,280 B (106.9 MiB) |

Frame-interval p95: 6.505 ms empty / 8.252 ms district. Counts include offscreen Actors/instances; draw calls and timings include terrain, lighting and UI. Resident memory includes newly loaded assets and shaders, not only compound component allocations. GPU frame time and dedicated GPU-memory deltas were not captured. Instancing avoids individual house Actors, but per-parcel material sections still produce substantial draw-call growth. These are local measurements, not a full-campaign performance acceptance verdict. [Raw samples and methodology](LabourHousingCompounds-Prompt4/Evidence/performance-samples.csv).

## Authoritative settlement measurement

Final selected native 1920 × 1080 replay, same camera, 180 warm-up and 180 sample frames per phase. Baseline is one completed stage-one compound plus market and access roads; comparison is six stage-one compounds plus market and an extended street network. Actor growth includes new road projections and presentation children. This complements the denser stage-three 32-parcel rendering benchmark above; it is not an isolated per-compound cost estimate.

| Metric (median) | One parcel settlement | Six parcel settlement |
|---|---:|---:|
| World Actors | 77 | 123 |
| Compound Actors | 1 | 6 |
| HISM instances / batches | 3 / 3 | 21 / 19 |
| Complete-scene draw calls | 240 | 528 |
| Frame interval | 5.984 ms | 6.170 ms |
| Game thread | 1.709 ms | 1.652 ms |
| Render thread | 6.020 ms | 6.099 ms |
| Process resident bytes | 4,685,742,080 | 4,687,118,336 |

Memory difference: 1,376,256 bytes (1.31 MiB); most assets were already loaded by the preceding upgrade flow. Measurements show local behavior, with no invented numerical acceptance threshold. GPU timing and dedicated GPU-memory allocation remain unmeasured.

## Final verification

[Developed parcel, 1920 × 1080](LabourHousingCompounds-Prompt4/Evidence/PlayerFlow-1920x1080/stage3.png) · [district](LabourHousingCompounds-Prompt4/Evidence/PlayerFlow-1920x1080/district.png) · [720p inspector](LabourHousingCompounds-Prompt4/Evidence/PlayerFlow-1280x720/stage3.png) · [evidence hashes and dimensions](LabourHousingCompounds-Prompt4/Evidence/manifest.json).


- Editor Development build: passed after the inspector correction and final capture adjustment.
- `Hansa.Compound`: 10/10 passed, including authored layouts, instancing support, full projection and rotated corner preview.
- `Hansa.Simulation.Population`: 13/13 passed, including stage progression, invalid upgrade atomicity, demolition cohort removal and post-demolition save/load checksum.
- `Hansa.Architecture.Authoring.EconomicSchemaCoverage`: 1/1 passed.
- `Hansa.Integration.Authoring.EconomicAssetReload`: 1/1 passed, including v19 discovery and preserved v18/older lineage.
- `Hansa.Compound.PlayerFlow`: passed at native 1920 × 1080 and 1280 × 720, with original PNGs and semantic snapshots. The corrected close camera repeats the original full flow. Native stage-three, placement, district and 720p inspector images were inspected without resampling.
- Win64 Shipping build and executable/receipt exclusion audit: passed (`20260915-184814928`). This is explicitly not a cooked-package/depot audit.
- `Hansa.UI.BuildMenu`: 6/10 passed. Four failures remain: BakeryFitsBesideRoad expects Ready but observes Blocked; CatalogAndLockedReasons rejects grain-farm cost text; SemanticsShortcutsAndFocus lacks the expected action family; ShorelineBuildingsUseAuthoritativeTarget cannot find a second unoccupied shoreline footprint. These concern wider existing flows and have not been established as pre-existing by a clean baseline. They remain visible release limitations. The new compound card count and actual compound player workflow passed.

No new raster imagery was generated for prompt 4. Component inventory: existing Homes tray/cards, placement ghost/grid, full-parcel selection, residence needs inspector and actions, save/load modal, district world presentation. All UI uses the existing approved system. Production assets are the four promoted compound definitions, twelve bindings and six technically corrected instancing materials. Screenshots under `Evidence/PlayerFlow-1920x1080` and `Evidence/PlayerFlow-1280x720` are verification references, not production textures. Native dimensions are preserved. The original prompt sets and R03 source assets remain under `SourceArt/Generated/Compounds/LabourCourts_20260915` and `SourceArt/Generated/Buildings/HansaLabourHousingKit_20260914_01`.

Commercial release acceptance is qualified by the wider build-menu failures, preview-alternative limitations, cohort-only navigation and unmeasured GPU/package gates above; these are not silently marked passed.


## Ordinary labour-house random selection — 2026-09-15 follow-up

The ordinary `Building.Residence.Laborer` construction card now selects a production stage-one compound before targeting. Eligible, unlocked and affordable compound families are drawn randomly without replacement from a presentation-session pool. All four families appear in each complete pool; a new pool is created when exhausted. Cancelling and starting a new choice consumes another pool entry. Explicit family cards remain available.

The resolved stable definition drives the ghost, 12/16 occupied cells, road orientation, costs and the ordinary authoritative placement command. Continuous click/drag or repeat placement draws the next family only after successful construction. Rejected placement, pointer movement, rotation and repeat toggling preserve the current family. Repeat retains the chosen road orientation. The existing deterministic parcel seed controls variations within each family; upgrades and save/load retain the placed identity. The uncommitted shuffle pool is session-local and is not gameplay identity.

This is a native build-presenter behavior change using already approved models and artwork. No asset definition or catalog fingerprint changes; existing residences are not resized or replaced. The normal card's tooltip explains random courts and 16 × 12 / 16 × 16 m parcels. All existing UI surfaces and states are reused.

The new `Hansa.Compound.RandomLabourConstruction` regression covers two complete shuffle cycles, rejected-placement stability, rotation stability, full parcel size, normal gateway commit, continuous selection and explicit-family selection. The compound suite now passes 11/11 (`Saved/CompoundIntegration/Tests/20260915-190407415-automation-Hansa.Compound`). Its projection fixture explicitly unregisters its own temporary primary asset so production-registry tests can run afterward. The live PlayerFlow district now uses the ordinary labour-house button five times and asserts all four families are built, followed by normal save/load of the random district.


Follow-up verification completed: the final native 1920 × 1080 PlayerFlow passed,
including the random district save/load assertions. Its unobstructed district image
was inspected at original resolution. The current Win64 Shipping build and
executable/receipt exclusion audit passed (`20260915-190650812`); the scope remains
binary/receipt rather than a complete cook/package audit. [Random labour-house
district](LabourHousingCompounds-Prompt4/Evidence/RandomLabour-1920x1080/district.png)
and test/build receipts are preserved under `Evidence/RandomLabour-1920x1080`.
