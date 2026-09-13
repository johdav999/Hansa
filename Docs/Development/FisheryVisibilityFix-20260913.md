# Fishery visibility repair — 2026-09-13

## Root cause

`Building.Fishery` resolved to `/Engine/BasicShapes/Cube.Cube` because no promoted
Fishery presentation mesh existed. The world-projection production state deliberately
hides unavailable fallback geometry. Simulation, selection and the production inspector
therefore continued to work while the completed building had no visible geometry.

## Fix

- Added `/Game/Mesh/hansa-fishery/SM_HansaFishery`.
- Bound `DA_Building_Fishery` and the MVP seed definition to that mesh.
- Advanced the definition authored revision to 4; save-time hashing refreshed the
  derived content hash.
- Imported 1254 x 1254 ImageGen oak and thatch base-color sources and wired them into
  the Oak, WetOak and Thatch materials.
- Generated three mesh LODs, four simple convex collision hulls, and retained the
  authored eight material slots.
- Added `Hansa.Content.Fishery.PresentationAsset` regression coverage.

## Accepted asset

- Bounds: 1124 x 742 x 492 cm; grounded at local Z=0.
- Presentation envelope: 3 x 2 cells.
- LOD triangles: 24,792 / 12,396 / 6,198.
- Water-facing authored direction: -Y.
- Roles: timber workhouse, steep thatch roof, covered porch, landing, net racks,
  winch, crates and fish cargo.

The accepted Blender master, FBX/GLB exports, ImageGen prompt records, evidence, and
1200 x 800 inspection renders are in
`SourceArt/Generated/Buildings/HansaFishery_P11_20260913/`.

## Historical scope

The asset is an original evidence-informed reconstruction. Official Lübeck descriptions
of the Gothmund fishers' settlement support the shore-facing thatched cottages, broad
equipment porches, sheds, net work and landings. Because the documented surviving fabric
is mostly later, those details are treated as functional and regional analogies rather
than an exact medieval replica.
