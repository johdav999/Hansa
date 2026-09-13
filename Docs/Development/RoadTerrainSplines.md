# Terrain-following spline roads and RVT shoulders — 2026-09-11

## Scope and contract amendment

Road presentation now derives canonical runs from the authoritative cardinal road grid. Simulation IDs, placement rules, costs, logistics, definitions, catalog hashes and save schemas are unchanged. This implementation explicitly supersedes the P18 prohibition on runtime road fitting. The approved source meshes retain their native 400 cm XY datum, pivots, silhouette, texture sources and material slots. Runtime deformation is a cosmetic projection of that source geometry.

This is a road rendering implementation, not a new road-art family. No provider calls, ImageGen generations, raster-art resizing, texture replacement, generated-media promotion or gameplay migrations were performed. The existing six approved meshes and earth textures are reused. Captures are non-shipping evidence, not production textures.

## Component inventory

| Component | Responsibility |
| --- | --- |
| `BuildRoadRuns` / `RoadRunNeighborMasks` | Canonical paths between endpoints/junctions, isolated cells and closed loops; derive the masks used by constructed road actors |
| `UHansaRoadSplineComponent` | Runtime spline grade, adaptive straight sections, residual terrain fitting, cached height data and RVT invalidation |
| Six existing road source meshes | Isolated, end, straight, corner, T and cross silhouettes; unchanged source assets |
| `M_Road_Terrain` | Existing earth shading plus a sampled residual height field and exact source-based XY boundary reconstruction |
| `RVT_HansaRoad` and transient volume | Road-only RVT writes, bounded invalidation and volume expansion as terrain streams in |
| Existing Landscape masters | Receive road color and roughness through the RVT mask; retain their underlying material and provide a virtual-texturing-disabled branch |
| Placement ghost sections | Same fitting code and topology, explicit preview clearance, no RVT writes |
| Editor commandlet and validation | Repeatable graph amendment, original-package backups, exposed component settings and deterministic material checks |

## Geometry and lifecycle

Runs are split at existing grid-cell boundaries. Straight cells use one 4 m spline section, or two 2 m/four 1 m sections when the sampled centreline exceeds the authored cubic-fit error tolerance. Reusable child components provide these sections. Junctions retain their approved silhouettes and use the same residual height correction as straight roads.

The source LOD0 grid is 12.5 cm. Each cell samples a 35 by 35 height field, including a one-vertex apron beyond every cell boundary. Midpoint probes check between source vertices; conservative corrections consider both triangulation diagonals. The same neighbouring samples are available on either side of a shared edge. Source-based XY reconstruction prevents the road crown from shifting shared boundary vertices sideways when a spline tilts. A 3 cm clearance and 2 cm alpha-weighted crown keep the physical surface above the sampled terrain. Preview clearance is 6 cm. The existing world-space earth texture coordinates preserve texture density across sections and junctions.

Native Landscapes use Unreal 5.8.2's complex collision-heightfield API after one terrain trace identifies the Landscape. Tagged alternative terrain meshes use the existing filtered collision traces. Roofs and untagged props are excluded. LOD0 is retained for fitted roads because the old decimated LODs cannot guarantee the same terrain conformity. This trades some distant geometry cost for consistent fitting.

An unchanged component transform, mesh, clearance, tolerance, material and RVT mode reuse the previous fit without any height traces or texture updates. The projection manager updates changed roads and affected neighbours; ordinary production events do not rebuild unrelated road geometry. Save/load reconstructs the derived runs and components from the original authoritative data.

Level add/remove delegates schedule fitting after collision registration completes. Changed Landscape bounds filter relevant road cells. Runtime-created RVT volumes are transient and grow when newly streamed terrain extends their bounds. Explicit terrain-edit tooling can call the reflected `FitTerrain` function with `bForce=true`. There is no per-frame terrain tracing or runtime Landscape sculpting.

Missing collision on a legacy map retains the legacy road datum. A partially loaded terrain field is diagnosed and withheld from the main pass until streaming completes. Runtime diagnostics expose fit status, reason, trace count and duration. This finite-resolution surface is intended for Landscape-scale terrain, not overhangs or arbitrarily thin collision spikes.

## Materials and authored settings

New production material packages:

- `/Game/Mesh/hansa-dirt-road/Materials/M_Road_Terrain`
- `/Game/Mesh/hansa-dirt-road/Materials/RVT_HansaRoad`

Existing amended Landscape masters:

- `/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/Materials/M_Terrain_Hansa_Master`
- `/Game/Hansa/Generated/Staging/LubeckWorldArt_P30/M_Lubeck_Ground`

Their existing staging status is unchanged. No production reference to staging was introduced. The original `M_Road_Earth` remains unchanged. Original amended packages are backed up under `Saved/RoadTerrain/Before/` before their first save.

The RVT uses YCoCg base color, normal, roughness, specular and mask storage. The road writer supplies earth color, roughness, coverage and mask; Landscape receivers blend color and roughness while retaining their existing normal shading. Roads do not sample this RVT, and the Landscape does not write to it, so there is no feedback loop. Physical roads always remain eligible for the main pass. Geometry fitting is independent of RVT availability.

Placement previews cannot paint the Landscape. Removal, visibility changes, replacement, refitting and wetness changes invalidate the affected world bounds. `hansa.Road.RVT 0` disables road RVT writes when sections are fitted; the capture explicitly refits to verify this mode. Disabling virtual-texture support uses the receiver material's feature-switch fallback.

The reflected component exposes `SurfaceClearance` (0.5–10 cm) and `SplineErrorTolerance` (0.25–5 cm), with units, tooltips, clamps and AI-access metadata. Road asset validation rejects invalid settings. These are presentation settings in the ordinary Blueprint/component Details workflow, not a second economic-definition schema. No gameplay migration is required.

## Reproduction and evidence

Build the Development Editor, then run:

```powershell
./Scripts/ConfigureRoadTerrain.ps1
./Scripts/RunAutomationTests.ps1 -TestFilter Hansa.World.RoadTerrain -SkipBuild
./Scripts/RunAutomationTests.ps1 -TestFilter Hansa.World.Road.CommandProjectionJourney -SkipBuild
./Scripts/RunAutomationTests.ps1 -TestFilter Hansa.World.Road.ProductionProjection -SkipBuild
./Scripts/RunAutomationTests.ps1 -TestFilter Hansa.World.Terrain.Construction -SkipBuild
./Scripts/CaptureRoadTerrain.ps1 -Width 1920 -Height 1080
./Scripts/CaptureRoadTerrain.ps1 -Width 1280 -Height 720
./Scripts/Build.ps1 -Target Hansa -Configuration Shipping
./Scripts/VerifyShippingExclusion.ps1 -SkipBuild
./Scripts/RunMediaCookAudit.ps1
```

The material commandlet backs up existing packages and amends graphs idempotently. Its command-line-only GameFeatureData setting follows the existing project's commandlet workaround; it does not change gameplay configuration or spend provider credits.

The terrain tests cover planar grades and cross-slopes, shared boundaries, preview/no-RVT behavior, cached fitting, terrain changes, missing terrain, adaptive splitting, material parenting, shader coordinate wiring, absence of RVT feedback, junction runs, closed loops and insertion-order independence. Existing command tests cover preview/final masks, neighbouring junction updates, invalid strokes, cancellation, removal and save/load restoration.

The viewport test enters the actual game through the frontend, selects a free area with terrain height variation, draws roads through ordinary placement intents, captures the previous rigid presentation in memory, then captures fitted roads, gameplay zoom, junction removal, save restoration and RVT-disabled geometry. It checks actual RVT writer registration. It does not save the game map or replace its terrain with a synthetic render fixture.

Final native PNG captures and the evidence index are retained in `Docs/Images/World/RoadTerrain/` and `Docs/Development/Evidence/RoadTerrain-20260911.json`. The PNGs are not resampled. Timing rows are measured forced full resamples of the visible road network; ordinary unchanged projections reuse cached data. The initial broad-phase implementation measured approximately 714 ms for 74 roads. Direct Landscape heightfield access reduced a comparable forced pass to approximately 29–38 ms; see the final evidence index for the final run rather than treating this as a universal performance guarantee.

## Acceptance limits

Road-specific automated checks and inspected game captures are distinct from full-game MVP acceptance. Existing competing directional-light warnings and pale scene exposure remain outside this change. The survey/P30 maps keep their existing staging status. The Shipping checks cover runtime compilation, binary/receipt exclusions, production references and expanded cooked Lübeck packages; they do not establish a clean-checkout packaged-game playthrough or final IoStore distribution acceptance.


## Changed implementation paths

- `Source/Hansa/Public/World/HansaRoadSplineComponent.h`
- `Source/Hansa/Private/World/HansaRoadSplineComponent.cpp`
- `Source/Hansa/Public/World/HansaRoadRuns.h`
- `Source/Hansa/Private/World/HansaRoadRuns.cpp`
- `Source/Hansa/Private/World/HansaRoadPresentation.cpp`
- `Source/Hansa/Public/World/HansaBuildingWorldProjection.h`
- `Source/Hansa/Private/World/HansaBuildingWorldProjection.cpp`
- `Source/Hansa/Hansa.Build.cs`
- `Source/HansaEditor/Public/World/HansaRoadMaterialCommandlet.h`
- `Source/HansaEditor/Private/World/HansaRoadMaterialCommandlet.cpp`
- `Source/HansaEditor/Private/Tests/HansaRoadTerrainMaterialTests.cpp`
- `Source/HansaEditor/Private/Tests/HansaRoadKitTests.cpp` (contract assertion wording)
- `Source/HansaTests/Private/World/HansaRoadTerrainTests.cpp`
- `Source/HansaTests/Private/World/HansaRoadTerrainCaptureTests.cpp`
- `Scripts/ConfigureRoadTerrain.ps1`
- `Scripts/CaptureRoadTerrain.ps1`
- `Config/DefaultEngine.ini`
- `Config/DefaultGame.ini`
- `Docs/Development/EnhancedMvpRoadKit.md`
- `Docs/Development/TerrainPlacement.md`

The viewport fallback test disables RVT road writers and refits. A separate application launch with project-wide virtual texturing disabled, an exhaustive rendered synthetic crest/dip/cross-slope matrix, and a dedicated World Partition unload/reload stress test were not run. Synthetic collision tests cover flat and sloping surfaces and an adaptive ridge; the rendered journey uses the existing uneven Landscape. The finite sampling grid is not a proof against arbitrary sub-grid terrain discontinuities. These limitations should remain visible in release acceptance.

The before capture uses the same earth shader with fitting disabled and the former single-normal rotation, rather than switching to an incompatible static-mesh-only material on a spline component. Both comparisons retain the existing scene exposure. A startup warning about the original mesh's material lacking spline usage may appear before the terrain material is assigned; the shipping terrain material has the required usage and final captured roads do not use a checkerboard fallback.
