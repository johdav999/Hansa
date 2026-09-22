# Market road transport range — 2026-09-16

Completed market-access providers have an inclusive MaximumMarketRoadDistanceCells limit, default 40 cells (160 m on the 4 m grid). Distance is the shortest path over completed orthogonally connected roads, plus one entrance step at each endpoint. Straight-line proximity never grants access. An unfinished market or unfinished road does not grant access. A market itself is eligible at its own endpoint.

Shared city inventories (no BuildingId) are served through any completed road-connected market, choosing the shortest eligible route. Inventories explicitly bound to a building remain physically bound to that market; another market cannot unlock their stock. Markets themselves require only their own road connection, never another market. Production request selection tries reachable inventories; building status tries every city market and selects shortest eligible distance with stable ID tie-breaking. Existing households rebind to an eligible market if their previous market becomes inaccessible. Two building endpoints must fall inside one operational market's reach. Inter-market stock transport still requires a physical road route.

The same query governs dispatch, pickup, in-transit revalidation, population and read-only projections. Over-range routes report MarketNotInRange with BuildCloserMarketOrShorterRoad. Loaded cargo pauses without disappearing and resumes after route recovery. Local production may finish using already delivered input, but external goods movement is blocked.

## Authoring and compatibility

MaximumMarketRoadDistanceCells is in Building / Logistics, with units, tooltip, range 2–4096, validation, reflection-generated Details/JSON/AI schema, compiler copying and seed copying. Authoring validation explains effects on population, transport and saves. No provider integration is added.

This is an additive default migration: old binary building assets and fixtures inherit 40 without changing IDs or catalog lineage. Non-default overrides participate in definition hashing and use the existing exact-hash save policy. Existing saves inherit the new finite reach on their next simulation update; cargo/inventories are preserved. The appended failure enum preserves all older numeric values and is accepted by save validation. No save envelope layout changes.

## Component inventory and states

- World warning: one imported static mesh, visible over completed road-requiring buildings lacking market access; hidden while constructing or recovered. Rotation is 45 degrees/second around Z. Reduced motion freezes rotation while preserving visibility. It is non-colliding and does not affect navigation or selection.
- Existing road warning: retained; if both apply, the market warning is offset so the two do not overlap.
- Existing inspector: native localized Market not in range cause/remedy text. Market Details states authored distance in cells/metres. No new screen shell, controls, table, chart or raster artwork.
- World semantic tag: Hansa.Status.MarketNotInRange; Blueprint read-only visibility query; existing automation market-access payload includes MarketNotInRange failure/remedy.

## Model and sources

The user explicitly requested headless Blender for the missing 3D symbol. This is a procedural 3D model, not a GUI raster. No ImageGen, provider calls or textures are needed. Existing navy/brass/chalk/amber palette and the road-warning family guide the shape.

Source: SourceArt/Generated/Props/HansaMarketRangeSymbol_20260916/. Editable .blend, FBX, GLB, 768-square native review render, scripts and import evidence are retained. Model dimensions are 163.462 × 163.462 × 39 cm, with 1,708 triangles and four constant PBR material slots. Runtime world scale is 2.25, following the existing road warning's readable city-camera treatment. No raster resampling is involved.

Staged import: /Game/Hansa/Generated/Staging/MarketRange_20260916. Production mesh: /Game/Mesh/hansa-market-range-symbol/SM_HansaMarketNotInRange. All four production materials are under the same production folder; no staging material dependency. The selected source was visually inspected before importing. The render is a review reference; the mesh and materials are runtime assets.

## Validation

Focused tests cover inclusive boundary, one cell beyond, source/destination direction, winding detour versus straight-line proximity, closer-market recovery, preventing remote-stock bypass, stable discovery order, loaded cargo pause/recovery and save decoding, authoring metadata/hash/range checks, warning rotation, visibility and collision. The real viewport test constructs an unserved bakery then completes a market through gameplay commands and captures matching inspector/world states.

Final execution: HansaEditor Win64 Development non-unity build passed. All 34 focused regression tests passed, plus the real-viewport acceptance test at native 1280x720 and 1920x1080. The two previously detected issues (new enum save-codec bound and construction-versus-operational-status expectation) are fixed and verified. Test names are in [tests.json](Evidence/MarketRoadRange_20260916/tests.json).

Four screenshots and matching semantic text prove missing-market and closer-market recovery states; see [native capture audit](Evidence/MarketRoadRange_20260916/viewport-review.json). Visual review accepted symbol visibility above the roof, clear Market not in range wording, absence after recovery, and no HUD overlap at the captured normal zoom. The initial close-zoom diagnostic capture was replaced with correctly framed evidence; the model itself was unchanged.

[Production asset audit](Evidence/MarketRoadRange_20260916/asset-audit.json) reloads the saved mesh and confirms native bounds and four production-folder material references. No staging material dependency exists.

Repeat via Scripts/VerifyMarketRoadRange.ps1. Validation used the existing isolated verification project's binaries, with Source and Content junctions to the main repository. The main editor was left running without discarding user state; rebuild/restart it to load the changed C++/reflected property. A full Shipping cook/package and exhaustive surveyed-map/zoom coverage were not run for this feature. This evidence does not claim the entire integrated MVP release gate passes.

![Missing-market warning at 720p](../Images/UI/MarketRange/market-range-1280x720-missing.png)

![Recovered market access at 1080p](../Images/UI/MarketRange/market-range-1920x1080-recovered.png)

## Second-market regression correction

The original range implementation incorrectly treated unbound shared city stock as stock bound to the oldest market. Consequently a second market neither restored service to a distant farm nor satisfied its own consumer-style market check. Shared inventory endpoints now resolve separately for each candidate market, and the selected route ends at that actual market entrance. Explicit physical bindings remain enforced. Provider projections depend only on road access.

ConstructionCompleted and BuildingRemoved already synchronize every building projection; the corrected authoritative query therefore refreshes existing buildings and their warning symbols immediately without a timer or new save fields. Regression coverage constructs two markets and a farm across an over-range winding road, verifies the unfinished second market does not grant access, then checks completion and removal. Gameplay data and authoring schemas are unchanged by this correction.

Correction validation: Development editor build passed; all 35 focused tests passed, including SecondMarketRefreshesRange. Evidence: [second-market-fix-tests.json](Evidence/MarketRoadRange_20260916/second-market-fix-tests.json). The running main editor was left intact; it must load rebuilt binaries for the fix to take effect.
