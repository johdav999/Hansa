# Campaign spline rivers — 2026-09-19

User request: replace square-cell rivers with continuous curved surfaces following
geographical paths, smoothly varying widths and continuous downstream elevations.
This is a staged world-map revision, not promotion into the playable MVP.

## Implementation

- Target: `/Game/Hansa/Generated/Staging/HansaWorld_20260918/L_HansaWorld_WP`.
- Existing Natural Earth 1:10 million river vectors and supplemental OSM vectors,
  projected EPSG:3035, use the same 40:1 campaign compression and city radial warp.
  NOAA ETOPO/EGM2008 remains the regional elevation evidence (8:1 vertical compression).
  Natural Earth is public domain; existing OSM ODbL attribution/source records remain
  in the original source package. No new downloads or raster artwork.
- Named, non-canal river lines are merged by name, clipped to land and split at
  lakes. Duplicate endpoints share one elevation. Braided paths have distinct IDs.
- 1.5 game-metre line simplification, maximum 80m control spacing, native cubic
  XY tangents. Elevations use a monotone fit and explicit cubic tangents in Unreal
  spline-key space, preventing uphill interpolation overshoot.
- Width varies smoothly from 36 to 40 game metres; depth is 3m and the nominal
  velocity is 0.5m/s. These are gameplay assumptions, not measurements. Direction
  is inferred from endpoint elevations; flat/tidal reaches are not flow-certified.
- Native `AWaterBodyRiver` actors expose editable spline, width and depth metadata.
  Unreal's native spline-generated static water surfaces avoid the coarse
  campaign-sized WaterZone grid. Endpoints are shared in position, height and
  tangent within each reach. Existing water shading is reused. A mesh-override
  approach was rejected: UE 5.8 clears overrides in its legacy PostLoad migration.
- Lake-only cells are separated into replacement custom water actors. The old
  mixed inland-water actors are retained disabled under `Water/LegacyDisabled`
  for recovery/comparison, never layered visibly beneath rivers.
- Automatic Water-brush carving is disabled to protect the accepted Landscape.
  A deterministic bed-only correction is on `SplineRiver_Hydrology`; dry city
  cores are protected. Native bank-falloff settings remain available but do not
  drive this offline correction. Rebuild the source package to regenerate it.
- Lighting, ground textures, trees, city markers, survey and original grading
  layers are not replaced. Runtime gameplay schemas/navigation are unchanged.

## Reproduce and verify

With the editor closed:

```powershell
python Scripts/HansaWorld/rivers.py
python Scripts/HansaWorld/sea_corridors.py
python -m unittest discover -s Scripts/HansaWorld -p test_rivers.py -v
./Scripts/Build.ps1 -Target HansaEditor -EngineRoot H:/Unreal/UE_5.8
./Scripts/BuildHansaWorld.ps1 -EngineRoot H:/Unreal/UE_5.8 -SplineRivers
./Scripts/ReviewHansaWorld.ps1 -EngineRoot H:/Unreal/UE_5.8 -Repair
./Scripts/ReviewHansaWorld.ps1 -EngineRoot H:/Unreal/UE_5.8
```

River import uses NullRHI for deterministic authoring. The explicit GPU repair
builds and saves native water surfaces and the merged Landscape in a full editor
rendering context; an independent fresh reopen validates persistence. The review
asserts river count, visible native surfaces,
monotone height interpolation, disabled legacy tiles,
832 loaded Landscape components, preserved city markers and expected merged heights.
It captures a close Lübeck river view as well as regional/city views.

Source outputs: `SourceArt/Terrain/HansaWorld/Prototype_20260918/SplineRivers_v1/`.
Hashes, assumptions, output sizes and bed correction magnitude are in `rivers.json`.
Test output/captures: `Saved/GenerationJobs/HansaWorld_20260918/review/`.
Original map/actors backup: `Saved/GenerationJobs/HansaWorld_20260918/before-spline-rivers/`.
Do not restore the backup over an open editor or later unrelated user edits.

## Limitations / acceptance

This revision addresses river surfaces, not higher-resolution banks, new wet-bank
materials or improved water shading. The 18.6m Landscape grid still limits bank
silhouettes. Lake shorelines keep their previous cell resolution. Modern regional
vectors are not verified medieval geography; width, depth, level and flow need
local evidence for production. Separate named-source confluences and lake/sea
mouths require visual inspection; this is not a certified ship-navigation graph.
Native spline components add rendering/editor cost; full campaign performance and
Shipping cook are separate gates. Staging remains NeverCook, with no production
reference or gameplay map change.

## Initial validation and visual findings (before approved cutouts)

- Four offline tests pass: source/output hashes, monotone cubic profiles and shared
  endpoints, preserved dry city cores, and separate lake-cell inventory.
- 180 river reaches, 15,973 spline controls; 32,045 mixed river cells removed from
  the active inland-cell output, with 134,146 lake cells retained.
- Rendered repair passed at `Saved/BuildArtifacts/20260919-105603098-hansa-world-review`.
- Independent read-only reopen passed at
  `Saved/BuildArtifacts/20260919-105949481-hansa-world-review`.
- Native readback: 832 Landscape components; maximum height-code error 1
  (3.125cm); maximum sampled uphill numerical rise 0.000693cm, below 1mm tolerance.
- Explicit river/lake/sea isolation captures show that the original map-wide sea
  planes also fill the below-sea-level inland riverbeds. They preserve a stepped
  outline around the new curved river surfaces. This is a **remaining visual
  defect**, not acceptance of the final shoreline appearance.
- The read-only zone audit confirms all sea/lake actors use custom meshes and no
  WaterZone assignment. No WaterZone was disabled and no actor was deleted. The
  broader proposed mutation was safety-rejected; the implemented narrower change
  retains old actors, with persistent editor and game hiding.
- Completing the visual replacement requires separately approved sea-mesh
  cutouts under inland river corridors, not disabling the sea. Approval has been
  requested. Until then, sea geometry is unchanged and visual acceptance remains
  incomplete. River-only normal rebuilding is included in the explicit repair
  path because native Water-generated normal overlays can contain zero normals.

The prototype is not production-promoted or navigation/performance/cook certified.

Latest river-only normal-rebuild/save pass:
`Saved/BuildArtifacts/20260919-110314530-hansa-world-review` (structured checks pass).
Isolated `RiverNoSea.png` and `RiverOnly.png` prove the stepped surrounding water
comes from the sea planes, while native river edges are curved. Bright triangular
artifacts remain at some tight bends even after normal rebuilding; those also need
correction before visual sign-off. Do not present the passing automation result as
completed visual acceptance.

## Approved inland sea cutouts and reviewed Trave correction

The user approved the narrowly scoped sea-corridor change on 2026-09-19.
`sea_corridors.py` subtracts buffered native spline paths from the existing sea
tiles, restricted to warped geographical land and excluding lake polygons.
No WaterZone is disabled. All 48 original sea actors remain visible; 33 receive
versioned, content-hash-suffixed replacement meshes. Original sea mesh assets are
retained. Open-sea geometry outside the approved inland mask and all lake actors
remain unchanged. Constrained polygon triangulation preserves holes; generation
checks retained/removed area accounting, containment and lake exclusion.

The first full-water capture after this change removed the stepped surrounding
water but exposed folded Trave ribbon triangles. The reviewed Trave now samples
the source polyline at approximately 12m spacing, applies at most 12m control-point
smoothing with fixed endpoints, limits tangents to local spacing and narrows
widths where necessary to keep half-width below 60% of bend radius. Width changes
are limited to 0.25m per metre of travel. This is an explicit cartographic/gameplay
approximation, not newly surveyed bank geometry. Heights remain monotone and
source reach identities remain stable. Other rivers retain their previous
planforms; this is not a claim that all remote river bends have been visually
approved. The fold-safety test densely checks the refined Trave spline.

The cityterrain workflow keeps original geographical inputs immutable, separates
the derived draft, preserves dry city cores, and requires a rendered repair plus
independent normal-open verification. Sea import validates all exact actors and
mesh bounds before changing any sea actor; generated source/output hashes are
checked before launching authoring. No production promotion is performed.

### Final validation for this approved change

- Five offline tests pass, including pinned sea source/output hashes and the
  refined Trave's no-fold condition. Current total: 180 reaches, 15,984 controls.
- Editor build passed: `Saved/BuildArtifacts/20260919-112536834-build-HansaEditor-Win64-Development`.
- Import passed: `Saved/BuildArtifacts/20260919-112840391-hansa-world-map`.
- Full rendered repair/save passed: `Saved/BuildArtifacts/20260919-112956875-hansa-world-review`.
- Independent fresh read-only reopen passed: `Saved/BuildArtifacts/20260919-113301783-hansa-world-review`.
- All 48 sea actors visible; 33 hash-identified cutout meshes persisted. All 180
  river reaches, 832 Landscape components and city markers passed readback.
  Height-code error remained 1; maximum uphill numerical rise 0.00069277cm.
  Map check reported zero errors and warnings. Engine startup self-test diagnostics
  are distinct from the successful campaign review result.
- Native 1920x1080 `review/RiverClose.png` shows the sea-cell outline removed and
  the reviewed Trave ribbon without the earlier folded white triangles.
  `review/Overview.png` confirms the regional sea, islands and lakes remain.
  `review/BeforeTraveBendCorrection.png` retains the intermediate comparison.

Remaining visual limitations are explicit: coarse Landscape banks, abrupt ends
of some source line segments, and unreviewed remote river bends. No claim of
complete hydrological connectivity or production visual certification is made.
This approved sea-under-river overlap correction and local Trave bend repair are
saved in the staged map; original meshes remain recoverable.
