# Terrain-aware model placement

Construction previews, footprint cells, placed building projections, and each road-stroke tile query the loaded scene's terrain at their world XY coordinates. Construction completion and save restoration rederive presentation height from the same authoritative 2D placement; height is not added to the simulation or save schema. Translated city instances retain their local grid and model pivot offsets.

Buildings remain upright and retain their authored base offset. Roads now use terrain-following spline sections and a residual height field, with their legacy Lübeck shoreline shader displacement disabled on real terrain. The placement cursor queries terrain before selectable scene objects, so rooftops do not shift the placement cell. Rostock refreshes its prebuilt ground models after its streamed level becomes visible; the mill rotor follows the mill's ground sample. Dock, quay, hoist and mooring assemblies retain their shared harbor deck datum rather than snapping to the seabed. Vessel positioning remains tied to water/berth data.

## Scene authoring

Native Unreal Landscapes are recognized automatically. Alternative terrain meshes need query collision, WorldStatic or WorldDynamic collision object type, and the actor or component tag `Hansa.Terrain`. Do not put that tag on roofs, water, or selectable props. Both runtime and editor construction use the same sampler; no editor-only dependency is introduced. Ground queries use world-space vertical rays extending 10 km above and below the nominal datum. Missing collision preserves the legacy placement for maps without terrain.

This adjusts model placement; it does not flatten terrain or deform building foundations. The former rigid tangent-plane road limitation is superseded by [spline roads and RVT shoulders](RoadTerrainSplines.md). Buildings retain their existing upright placement contract.

## Verification

`Hansa.World.Terrain.Construction` uses real collision queries against an explicitly tagged ground mesh with a higher, untagged roof. It checks a translated city datum, road and preview height, repeated reprojection without drift, terrain changes, completion height and authored pivots for bakery/warehouse/residence, and road slope alignment. The fixture does not substitute for visual inspection of each future authored city Landscape.

### Results — 2026-09-10

Editor DebugGame build passed. Thirteen targeted tests passed:

- `Hansa.World.Terrain.Construction` (1): `Saved/BuildArtifacts/20260910-191821781-automation-Hansa.World.Terrain/`.
- `Hansa.World.Road.CommandProjectionJourney` (1): `Saved/BuildArtifacts/20260910-192050434-automation-Hansa.World.Road.CommandProjectionJourney/`.
- `Hansa.World.Road.ProductionProjection` (1): `Saved/BuildArtifacts/20260910-192106252-automation-Hansa.World.Road.ProductionProjection/`.
- `Hansa.World.Rostock` (2): `Saved/BuildArtifacts/20260910-192011255-automation-Hansa.World.Rostock/`.
- `Hansa.UI.World` (8): `Saved/BuildArtifacts/20260910-192121959-automation-Hansa.UI.World/`.

An initial broad road filter also selected five review-capture tests that require an open isolated review scene and failed that prerequisite; the construction and production road tests were rerun individually and passed. The first UI/world run recorded eight successful tests but no final harness completion marker; the repeat completed successfully. No visual acceptance claim is made for future city terrain assets.
