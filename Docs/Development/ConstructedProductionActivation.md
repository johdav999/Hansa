# Constructed production activation — 2026-09-11

## Root cause

Player placement created a building and construction progress only. Completion changed its state to Completed but did not create a production unit or building inventory. Prebuilt scenario buildings received those records separately. ShowWorldBuilding therefore found no production projection for the completed bakery and rendered the generic legacy inspector with “Ready for operation.” The building also could not produce goods.

## Fix

The authoritative ConstructionAndProduction phase now synchronizes completed player constructions immediately after construction advances and before workforce allocation/production. A building with elapsed construction ticks, an authored recipe, and no existing production receives one active unit using the first compiled recipe and a real empty storage buffer at the authored capacity. Existing building storage is reused. City workforce is allocated normally; inputs, workforce, road access and delivery blockers remain authoritative. No goods or workers are granted.

Synchronization preserves existing units, recipe choices, pause state, inventory, reservations and progress. Entity IDs are allocated deterministically above existing IDs. Prebuilt scenario units with no construction history retain their explicit initialization. Older saves containing completed player constructions without production are repaired at the next simulation tick; the save schema and definition hashes do not change. Existing save validation remains authoritative: building production derives its city from placement.

No gameplay fields, editor/provider dependency, assets, widget appearance or schema were added. The existing authored recipe and storage fields drive this runtime lifecycle correction. The native inspector automatically receives its existing production projection and uses the approved compact panel.

## Components and visual evidence

Reused: compact inspector host, worker portrait, identity header, flour/bread ports, batch ring, Pause/Details controls, labor footer, and construction/detail/focus states. No image generation, raster edits, new prompt set or asset imports were needed for this implementation-only repair.

Real game viewport captures were generated through ordinary build-menu placement and world selection. Inspected native completed-bakery captures at 1280x720 and 1920x1080: compact panel, portrait, recipe ports, progress ring and controls present and within viewport, with readable labels and focus. Captures are evidence, not shipping assets:

- Saved/ProductionInspector/production-1280x720-built-bakery-construction-completed.png
- Saved/ProductionInspector/production-1920x1080-built-bakery-construction-completed.png
- Matching .tsv files contain semantic bounds and values.

The existing camera composition shows the map edge; these captures validate the inspector and do not constitute world-art acceptance.

## Verification

- Development build: Saved/BuildArtifacts/20260911-171007729-build-HansaEditor-Win64-Development
- DebugGame build: Saved/BuildArtifacts/20260911-171107952-build-HansaEditor-Win64-DebugGame
- 77 simulation tests passed: Saved/BuildArtifacts/20260911-171055276-automation-Hansa.Simulation
- ConstructedBakeryLifecycle, ProductStockAndTransit and RecipeAndInventoryTruth passed: Saved/BuildArtifacts/20260911-171010349-automation-Hansa.UI.ProductionInspector
- The new regression covers normal placement, exact completion tick, compact widget semantics, actual empty storage, workforce/blockers, pause persistence, idempotence and save/load.
- Real-viewport checks passed: Saved/BuildArtifacts/20260911-170510757-production-inspector-1280-720 and Saved/BuildArtifacts/20260911-170755374-production-inspector-1920-1080.
- Existing ClockPauseAndCompletion test remains failing because its initial scenario projection has no working batch after one tick. It does not construct a building; its fixture assumption was not changed as part of this repair.

## Applying to a play session

Restart the editor/game with the rebuilt binary, load the affected save, and allow one simulation tick. Existing player-built recipe buildings with missing production are activated and use the compact inspector. Already-running processes do not receive these binary changes automatically.
