# Hansa campaign geography — staged prototype

## Location and scope

Open `/Game/Hansa/Generated/Staging/HansaWorld_20260918/L_HansaWorld_WP` in Unreal 5.8. This is a separate World Partition map, not a replacement for the current Lübeck gameplay map. It is a geographic authoring prototype, not a production-approved campaign or a new integrated-MVP deliverable.

The roster contains 31 major Hanseatic network locations, including member cities, the four principal foreign kontor destinations, and Stockholm as a trading destination. These categories are recorded separately; the roster is not a claim that every destination was a League member or that it exhausts historical membership. City coordinates are approximate historical-centre seeds and need historical review.

Every centre has a 20 m reference cube and a native TextRender name, grouped under the editor-only `CityMarkers` data layer and `DEV_CityMarker_` / `DEV_CityLabel_` actor labels. These are references, not settlement buildings. No generated raster artwork was added.

## Geography and gameplay transform

- Source elevation: NOAA ETOPO 2022, 60 arcsecond bed dataset, EPSG:4326, EGM2008 vertical datum (EPSG:3855). Combined topography/bathymetry is coarse regional evidence, not a detailed city survey.
- Planar working CRS: EPSG:3035. Bounds: west 2,850,000; south 2,762,500; east 5,850,000; north 5,200,000 metres.
- Native Landscape: 4,033 × 3,277 vertices; 832 components, each 2 × 2 sections of 63 quads.
- Gameplay extent: 75 × 60.9375 km; 18.60119 m grid spacing. Horizontal compression is 40:1, with independent 8:1 elevation compression.
- Axes: X east, Y south, Z up. Native height decoding: `(uint16 - 32768) / 128 * 400` centimetres, actor Z = 0.
- Non-overlapping radial transforms expand local city surroundings while retaining each city's regional centre position. Per-city core and transition radii are recorded in the manifest. The GeoReferencing actor describes the source CRS only: its stock conversion does **not** invert the gameplay compression/warp.
- Dry city cores are flattened with smooth grading falloff. Water pixels remain protected. Source validation checks all 31 centres are dry and interior core slopes are at most 3 degrees.

The unchanged downloaded DEM and the projected survey raster are retained separately from gameplay-compressed terrain. Native Landscape edit layers preserve the locked `Survey_Base`, hydrology delta, and gameplay grading delta. Historical layers are placeholders: this is modern geography, not an evidenced medieval reconstruction.

## Water and vegetation

Natural Earth 1:10 million land, lakes and river centre-lines provide major coastline/island and inland-water shapes. Supplementary OpenStreetMap geometry adds Trave, Warnow and Pregel. Named modern canals are filtered out, but other modern changes have not been historically reconstructed.

Custom native Water Body actors render tiled sea and explicit inland surfaces over separately authored Landscape hydrology. This prototype does not yet provide directed River splines, fully validated flow/depth queries, ship navigation, buoyancy, or historical harbour connections. Lake levels, river widths and channel depths are inferred/gameplay-adjusted. Fine banks follow the ~18.6 m terrain grid and require refinement for close-up city play.

23,372 deterministic tree instances reuse the project's alder, willow, birch, oak and beech meshes, with young and mature variants. Forest placement is an artistic draft with slope, water and city-core exclusions, not measured medieval land cover. Staged material copies enable instanced rendering without changing the source tree materials. Instances are visual vegetation, not registered forestry gameplay resources.

## Reused appearance

- Terrain: `/Game/Hansa/Generated/Staging/LubeckWorldArt_P30/M_Lubeck_Ground`, including the existing shared multi-scale ground-patch improvements. No new texture palette or raster art.
- Water: existing native single-layer-water `M_Rostock_Warnow` from `Rostock_P31`, compatible with explicit Custom water meshes without a River WaterInfo-texture dependency. The initial Lübeck River material was replaced in staging during visual correction.
- Trees: `LubeckTrees_20260916/Meshes`, with map-owned instancing material variants.
- Environment actors are copied from the saved `L_Lubeck_Terrain_Preview_WP`: directional light intensity 5, temperature 6500 K, rotation (-60, -10.308601, 112.360672); skylight intensity 2; sky atmosphere, cloud and exponential-height fog. The original level is not saved by this importer.

## Reproduction and evidence

Configuration: `Scripts/HansaWorld/world.json`. Source package and manifest: `SourceArt/Terrain/HansaWorld/Prototype_20260918/`. City table: `city-centres.csv`. Source URLs, access times, hashes and licensing records are retained alongside downloads; OSM query results have separate provenance records.

From the project root, with the editor closed:

```powershell
python -m pip install --target Saved/GenerationJobs/HansaWorld_20260918/python -r Scripts/HansaWorld/requirements.txt
python Scripts/HansaWorld/prepare.py --download-only
python Scripts/HansaWorld/supplement.py
python Scripts/HansaWorld/prepare.py
python Scripts/HansaWorld/validate.py
.\Scripts\Build.ps1
.\Scripts\BuildHansaWorld.ps1
.\Scripts\BuildHansaWorld.ps1 -Finalize
.\Scripts\ReviewHansaWorld.ps1 -Repair
.\Scripts\ReviewHansaWorld.ps1
```

Downloads are opt-in; normal CI makes no live data calls. Import is editor-only and uses an offline commandlet because no connected Unreal MCP authoring endpoint was available. An existing map is reopened rather than blindly overwritten; changing the source configuration requires an explicit map revision workflow, not assuming a retry reimports it.

Source validation covers encoded layers, hashes, city grading, water clearance, tree ground positions and required river inventory. Opt-in automation `Hansa.World.Campaign.Review` reopens the map, checks the 31 cubes/labels and 832 components, and compares merged native Landscape heights against the source. First readback had a maximum error of one encoding quantum (3.125 cm). Render captures and logs live under `Saved/GenerationJobs/HansaWorld_20260918/review/` and `Saved/BuildArtifacts/`.

Successful numerical checks do not establish visual, navigation, streaming-performance or shipping acceptance. The staged directory is excluded from cooking by the existing project configuration. No production promotion, full Shipping cook, campaign integration, historical approval or target-hardware performance certification has been performed.

### Final verification, 2026-09-18

- Editor build succeeded: `Saved/BuildArtifacts/20260918-082143543-build-HansaEditor-Win64-Development`.
- Reopened-map automation passed: `Saved/BuildArtifacts/20260918-082150830-hansa-world-review`.
- Source hashes (including unchanged downloads and OSM supplements), 31 dry flattened centres, tree positions and encoded layers passed `validate.py`.
- Native merged Landscape readback: maximum one height-code difference, 832 components; 31 cube actors and 31 text actors present.
- Visually inspected overview, Lübeck, Lübeck close-up and Stockholm captures at 1920 × 1080. Water surfaces, land/island silhouettes, terrain variation and city reference cubes/text are visible. The review explicitly uses editor visibility because game mode hides editor-only markers.
- Remaining visual limitations: coarse stepped close-range riverbanks, pale/noisy offscreen shading and sparse regional forest coverage. These captures establish the geographic prototype and reference placement, not final art-direction acceptance. Wet-weather, low-angle shoreline, streaming performance and production packaging gates remain open.

Selected native-resolution review images are retained in `Docs/Images/Terrain/HansaWorld_20260918/`. These are actual Unreal captures, not generated mockups or production textures.

## Normal-open visibility regression — 2026-09-18

The initial review was insufficient: it force-loaded World Partition actors and called `ForceLayersFullUpdate`, but did not persist the resulting GPU-merged Landscape. Those operations hid two normal-open defects. The live editor had only 18 loaded actors and no LandscapeStreamingProxy actors while streaming was enabled; after disabling streaming it loaded 56 proxies, but the original NullRHI-authored surface still required GPU regeneration. `ALandscape::CanUpdateLayersContent` explicitly requires `FApp::CanEverRender()`.

The staged authoring map now uses World Partition with spatial streaming disabled. This is an intentional prototype policy, not a shipping streaming/performance solution. Both fresh builds and finalization retain that setting. The explicit `ReviewHansaWorld.ps1 -Repair` path merges terrain on a real rendering device and saves only map-owned terrain packages. Ordinary review no longer force-loads actors or requests a height-layer rebuild, and asserts that all 832 components are loaded on an ordinary map open. A separate normal review after the repair is required; a successful repair run alone is not persistence evidence.

Verified repair: editor build `20260918-085018359-build-HansaEditor-Win64-Development`, rendered repair `20260918-085253218-hansa-world-review`, independent read-only reopen `20260918-085433662-hansa-world-review`, all under `Saved/BuildArtifacts/`. The fresh-session test passed all 832 loaded components, 31 markers/labels, and maximum merged-height error of one encoding quantum. Its Lübeck capture shows exposed dry terrain and bounded water surfaces without either earlier test helper. Offscreen verification disables the MCP plugin for that process only to prevent external resource polling errors from contaminating automation results; the interactive editor retains MCP.

## Attribution

NOAA ETOPO data is available for private, academic and commercial use; retain NOAA attribution and dataset metadata. Natural Earth data is public domain. Supplementary river geometry is © OpenStreetMap contributors, licensed under ODbL; preserve attribution and assess derived-database obligations before distribution. Sources and hashes are in the source package, rather than relying on an unversioned web lookup during import.
