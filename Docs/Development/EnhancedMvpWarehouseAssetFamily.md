# EMVP-P16 — Warehouse family (in progress)

Implementation started 2026-09-08. **Not accepted, not imported, not promoted.**

Source job: `Saved/GenerationJobs/hansa-warehouse_P16_20260908/`. Retained delivery: `SourceArt/Generated/Buildings/HansaWarehouse_P16_20260908/` (see its README, evaluation and evidence).

Native `AHansaWarehousePresentation` defines Storehouse, Doors, Hoist, Skids and Cargo roles with no hardcoded content dependency. Cargo is a read-only, bounded occupancy projection from the existing inventory snapshot. The world projection manager updates it on synchronize/rebuild and inventory-only event consumption. No gameplay schema or persisted state changes; existing editor metadata, validation, migration and inventory import workflows remain applicable unchanged.

Native automation coverage added: `Hansa.World.Warehouse.CargoProjection` (unknown/empty/band thresholds/full/overflow/construction/blocked/idempotent instances/collision and navigation isolation). It has not run yet: linking is blocked by the existing Unreal Editor holding module DLLs.

Definition baseline remains Building.Warehouse, footprint 4×3, capacity 200000 milli-units, requires road, does not require shoreline, Engine Cube presentation. Its original SHA256 is recorded in the source provenance. Production resolver staging exclusions have not been weakened.

## Outstanding gates

1. Resolve Unreal's exit/save state and close the existing editor without losing desired edits. Computer Use failed twice with a sandbox ACL error; MCP initialization timed out after the close request. No force-quit was performed.
2. Build the final C++ changes and run Warehouse plus world-projection, logistics and save/load regression tests.
3. Import five verified modules/materials into `/Game/Hansa/Generated/Staging/Warehouse_P16/`; create the review Blueprint, generate/inspect three LODs, assign every slot, save, reopen and inspect.
4. Verify staged asset bounds, ground/attachment pivots, access collision, repeated-cargo performance, and neutral/warm real game-camera captures.
5. Complete actual Lübeck road connectivity, selection, cargo transfer state and save/load checks. Pure Blender reviews do not satisfy these gates.
6. Present the actual asset comparison for explicit production promotion approval, then connect the stable Warehouse presentation and rerun Shipping reference exclusion checks.

Do not mark EMVP-P16 complete or update the visible-content manifest to production-ready on the strength of these source files.
