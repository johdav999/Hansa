# EMVP-P20 / P30 / P31 — shared props and two-city assembly

Status: implementation in progress, 2026-09-08. No P20 assets promoted yet.

The user explicitly expanded P20 to include P30/P31 city assembly, rather than
accepting isolated lighting previews as the final scene gate. Promotion approval
is recorded in the task; technical and visual acceptance still precedes promotion.

## Acceptance boundaries

- P20: the fourteen existing shared cargo, vegetation and street-dressing roles
  in the P02 manifest; reuse approved cargo, timber and wagon geometry first.
- P30: preserve deterministic Lübeck construction and plot readability while
  replacing actual golden-path primitives and staging dependencies.
- P31: a geographically grounded, prebuilt, inspectable Rostock connected to
  existing markets and route stops, with construction unavailable.
- Do not label a kit preview as an assembled city, an ambient cart as simulated
  cargo, or a successful import as Shipping acceptance. P32/P34 continuous vehicle
  projection remains separate unless needed to satisfy an explicitly assigned gate.

## Initial inventory

Approved P15 cargo contains a crate and sack; P19 contains loaded sacks and a
modular wagon; P12/P13 contain independent logs and planks. These are reuse
candidates, not permission to overwrite their packages or source masters.
Remaining roles include a barrel, bench, public well, symbol sign, fence/gate,
period light, shoreline debris, tree, sparse ground cover and shore plants.

The current Lübeck foundation still references Engine basic shapes. The earlier
Lübeck terrain source has a saved World Partition Landscape and native Water
preview, but its later surface/water report explicitly retains historical,
material, licensing/datum and Shipping gates. Do not rely on its superseded
source-only STATUS.md, and do not promote it merely because packages exist.
No Rostock gameplay level was found in the initial inventory.

## Evidence and sources

Working evidence: `Saved/GenerationJobs/city-life_P20_20260908/`.
The model work follows HansaModels and P07's authored metre/centimetre, ground
pivot, 25/65/120 m camera and shared material contracts. Terrain follows CityTerrain.
Actual dataset, reference, geometry and validation records will be retained with
the selected source assets; this document is not an acceptance receipt.

## Continued implementation — terrain safety and prop review

- Seven street meshes are saved under `/Game/Hansa/Generated/Staging/CityLife_P20/Meshes`:
  barrel, bench, gate, fence, driftwood, balance-symbol sign, and well. All have
  three LODs, source-matching centimetre bounds and explicitly assigned existing
  P15 materials. They remain staged, not production-accepted.
- Packed editable master reopened and all consumed file images were verified packed.
  Native 1280 × 720 front/side/rear renders were inspected. Whole silhouettes and
  both sign faces are present; close-up/material/native-Unreal acceptance remains.
  These are three static viewpoints, not a completed animated turntable.
- `Scripts/HansaTerrainContract.py` provides offline, read-only validation of
  topology, bounds, origin, native height decoding, controls, source hash, byte
  count, path containment and NoData. Nine Python tests pass. A source preflight
  is explicitly not promotion or native scene acceptance.
- Rostock's original normalized 65535 encoding produced up to 0.001968384 m
  error under Unreal's actual decoder. `NativeEncoding_v2` preserves the measured
  TIFF unchanged and corrects only R16 encoding, reducing maximum full-grid error
  to 0.000968933 m. Original survey and earlier draft remain intact.
- Added `HansaCityTerrainToolset` in the Editor-only module. Its import operation
  accepts no arbitrary path/code: it pins the exact reviewed Rostock manifest and
  heightmap by SHA-256, refuses dirty packages/PIE/existing destinations, and creates
  only a staged native World Partition Landscape with GeoReferencing and separate
  survey/historical/gameplay layers. Its readback compares every measured-layer
  sample by hash. Native Water layers are reserved for the later water pass.
- After the user closed the editor, the full Development build linked successfully.
  The final build receipt is `Saved/P31-Terrain-Build-r3.log`. The registered native
  tool was loaded in a fresh Development editor on task-local MCP port 8002.
- The staged Rostock World Partition map now exists and was saved and reopened.
  Native measured-layer readback matches all 4,068,289 samples by SHA-256, with
  256 loaded components. Nine offline tests and both native editor guard tests pass.
  `Saved/GenerationJobs/city-life_P20_20260908/evidence/rostock-native-survey-reopened.json`
  records the clean, unchanged authoring context before and after verification.
- The first guard-test fixture incorrectly used an abstract asset under `/Temp`;
  Unreal did not count it as dirty, and the test unexpectedly created the authorized
  staged Rostock map. No existing city was overwritten. The corrected concrete
  fixture verifies that it is dirty before invoking the importer. Native inspection
  also now disables package dirtying explicitly. Both corrected tests pass.
- A saved technical lighting rig and native 2825 x 722 overview were inspected.
  The first overexposed capture was rejected; `rostock-survey-baseline-r2.png`
  shows the full Landscape without obvious missing tiles. It still uses default
  ground material and has no authored water: this is not final terrain acceptance.
- Seven props were placed at unit scale in a separately copied P15 review map,
  saved and reopened. All seven native closeups and the 25 m view were inspected.
  The technical checker floor is preview-only, not a golden-path scene.
- Native review found a twisted well axle and a driftwood ground-pivot gap.
  Lower LOD thresholds did not repair the axle; its cause was the inherited tube
  helper wrapping the final endpoint of an open two-point tube. The P20-local
  generator now uses one-sided endpoint tangents, with ring-planarity assertions.
  It also grounds the driftwood geometry at export origin. Approved older kits
  were not edited. Revision 6's source render, clean FBX/GLB reimport checks, and
  reopened native well/driftwood closeups confirm the geometry corrections.
  The separate `L_CityLife_Review_r6` map uses sibling `Meshes_r6` assets; previous
  drafts remain intact. Driftwood arrangement is still too regular for final art.
- Retained the draft master, seven FBX/GLB pairs, material maps, prompt, scripts,
  geometry receipts and inspected comparisons under
  `SourceArt/Generated/Props/HansaCityLife_P20_20260908/Review_r6`.
  Rostock native verification evidence is now also retained in the survey's
  `NativeEncoding_v2/NativeReview_20260908` folder. No final role or scene gate
  is implied by this recoverable checkpoint.

The editor-close prerequisite is resolved. Preserve the original Lübeck preview
and authoritative 240 × 160 m
placement grid: the 4.032 km surveyed context cannot silently replace gameplay
coordinates or construction rules. P20 vegetation/light/reuse bindings, P30 world
assembly, P31 rendered trade-city integration and all final gates remain open.
