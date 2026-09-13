# Lübeck staged terrain — 7 September 2026

This report supersedes the earlier source-only status for import, surface and water progress. This is a staged modern-data draft, not an approved medieval reconstruction or production-ready asset.

## Saved Unreal result

`/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/L_Lubeck_Terrain_Preview_WP`

The map was saved and reopened. Automated read-back confirmed 25 water actors, the assigned Landscape instance on every loaded Landscape proxy, a locked measured base edit layer, and the unchanged source R16 SHA-256. Map check returned zero errors and zero warnings. Machine-readable evidence: `staged-validation-20260907.json`.

Native World Partition Landscape: 2017 × 2017 vertices, 2 m spacing, 4032 × 4032 m coverage, 16 × 16 components with 2 × 2 sections of 63 quads. EPSG:25832; projected bounds [608850.5,5967914.5,612882.5,5971946.5] m. Projected origin [610866,5969930,0] m. Unreal X east, Y south. GeoReferencing is configured in Flat Planet mode. Dataset-specific vertical datum and redistribution terms remain promotion gates. Source grid/control-point details and original download hashes remain in `terrain-manifest.json`; its original source-prepared status is historical, not current editor status.

The original non-WP preview and measured R16 remain preserved. The measured edit layer is locked but still displayed with Unreal's default name `Layer`: its rename property is protected in the Python API. The separate native `Water` edit layer holds inferred channel carving. No gameplay map or approved production content was changed.

## Ground material

Staging Materials contains `M_Terrain_Hansa_Master` and `MI_Terrain_Lubeck`. Grass/loam and wet-bank/loam base colors are blended with a georeferenced OSM-derived wetness mask. The material authors roughness (.88 dry to .48 wet), specular .25, and distance fading of fine texture detail over approximately 200 m. Far-distance tint parameters are editable in the instance.

Generated base-color source masters are `../Materials/lubeck--grass-loam--v1.png` and `../Materials/lubeck--bank-loam--v1.png`, each native 1254 × 1254 pixels, generated with built-in ImageGen, copied unchanged into the repository. Sibling `.prompt.md` files preserve the prompt set. These are draft artistic base-color inputs, not measured PBR materials. No resampling was used. Native output inspections found usable grass/loam and fine bank soil without text or buildings. Final tiling, near-camera scale, lighting contamination and style-anchor approval are still acceptance work.

Imported base colors are sRGB. `T_Terrain_Lubeck_ShoreWetness` is a linear, clamped 2017 × 2017 mask. Its 4/10/20 m shore buffers are inferred wetness treatment, not measured land cover. Source vectors, mask generation, and water inventory are under `hydrology/`.

Limitations: no authored normal/height channels, painted Landscape LayerBlend/Layer Info assets, physical material classification, seasonal response or RVT. Grass coverage is an artistic placeholder, not a historical land-cover claim. The result does not yet meet the full CityTerrain material acceptance gate.

## Native water

One Water Zone and 25 native Water Bodies: 23 river/channel segments plus Krähenteich and Mühlenteich lakes. No Ocean actor: the Baltic coast is outside these compact city bounds. Small water polygons used in the shoreline mask are not all represented as separate native lake actors.

Projected centerlines and lake boundaries come from OpenStreetMap contributors, retrieved 2026-09-07 through Overpass. Attribution and ODbL obligations apply; see https://www.openstreetmap.org/copyright . Preserve `hydrology/osm-water-complete.json` and do not present OSM geometry as survey elevation or medieval evidence. Names containing canals, modern regulated reaches and the feature identifiers require historical review and eventual canonical Hansa IDs before promotion.

Draft surface levels: lakes approximately 2.32 m; Wakenitz/Dükerkanal 3.55 m; other river/channel segments .9 m in the working height frame. These are inferred level-group choices, not measured water gauges. Widths are estimates from centerlines and shoreline distance, clipped to 5–250 m; direction and confluence correctness remain unverified. River velocity is an inferred .2 m/s, lake velocity zero. Native metadata read-back passed after correcting Unreal struct-array copy semantics.

Channel depth parameters are 1.5 m, with 3 m curve ramps; these are inferred draft bathymetry on the separate Water layer. They do not establish actual depths throughout the river or validate navigation. Initial shallow-ground intersection was visibly corrected by native Water carving. A lake test emitted a dilated-mesh triangulation warning; later map validation alone does not prove this geometry issue is fully resolved.

Project-owned staged `M_Water_Hansa_Master` and `MI_Water_Hansa_Base` retain native Unreal Water shader functionality, with city Lake/River instances. Absorption/scattering/albedo are artistic inland-water settings, roughness .22, reduced normal strength, and foam opacity zero. Parameter values passed read-back. These optical properties are not field measurements.

## Remaining acceptance gates

- Fresh overview, shoreline, close-ground, confluence and underwater visual QA after final optical tuning; persistent capture and wet-weather/gameplay views.
- Refine broad masks, shore gaps, river width changes, triangulation, elevations at connections and historical canals; verify downstream ordering and exclusions.
- Verify final sculpted terrain delta and collision against the locked survey layer, not only source-file integrity and earlier pre-carve control tests.
- Complete terrain PBR/layer/physical-material work and native texture-scale testing.
- Verify World Partition runtime streaming, water queries/buoyancy if used, shader/mesh cost, target-platform cooking, and Shipping exclusion of staging/editor artifacts.
- Confirm dataset license and vertical datum, historical period and reconstruction decisions, then obtain explicit approval before production promotion.

Authoring adapters and validation scripts are in `Saved/GenerationJobs/LubeckTerrain_20260907/`. UE 5.8 material setters return false unconditionally in the checked engine implementation; successful optical authoring was confirmed by parameter read-back rather than that return value. Use `CaptureViewport` for fresh render QA; window screenshots sometimes retained an older scene frame.
