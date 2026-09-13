# Lübeck survey import — staged, incomplete

Imported through Unreal MCP Landscape UI and saved through Unreal's native editor Python API.

- Preview: `/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/L_Lubeck_Terrain_Preview`
- Native Landscape: 2017 × 2017 vertices, 16 × 16 components, 2 × 2 sections, 63 quads per section.
- Transform: location (-201550, -201650, 3200) cm; scale (200, 200, 25); zero rotation.
- Bounds: (-201550, -201650, -208.984375) to (201650, 201550, 2135.9375) cm.
- Axes: X east; Y south. Projected origin remains (610866, 5969930, 0) m, EPSG:25832.
- Saved and reopened successfully. Existing gameplay map was not modified.

## Collision control-point checks

Vertical traces from Z=10000 cm to -10000 cm:

| Point | Source m | Unreal m | Error mm |
| --- | ---: | ---: | ---: |
| NW | 19.09 | 19.0897949 | -0.205 |
| NE | 2.94 | 2.9394434 | -0.557 |
| SW | 7.68 | 7.6795898 | -0.410 |
| SE | 9.99 | 9.9902246 | 0.225 |
| Centre | 11.11 | 11.1093164 | -0.684 |

## Not yet complete

- User-created template is a non-World-Partition map; conversion is outstanding.
- GeoReferencingSystem class unavailable in current editor. Plugin setup needs approval before changing project configuration.
- Realistic terrain material, material layers, historical edits, and water actors are not authored yet.
- Survey edit-layer naming/locking not yet verified.
- Source datum and dataset-specific reuse terms still need final verification; no production promotion.
- No performance, cooking, or Shipping acceptance claim.
- Basic template actors remain; this is not a final terrain-only scene.

## Hydrology preparation

Downloaded `hydrology/osm-water-original.json` from Overpass API on 2026-09-07. Original OSM geometry is preserved, not yet imported. Attribution: © OpenStreetMap contributors, ODbL; https://www.openstreetmap.org/copyright.
Query bounds: south 53.845, west 10.65, north 53.887, east 10.724. Query: natural=water ways/relations and river/canal ways; tags and geometry output.
Modern geometry is not a medieval reconstruction. Municipal reference: https://www.luebeck.de/de/rathaus/verwaltung/umwelt-natur-und-verbraucherschutz/wasser/oberflaechenwasser/oberflaechenwasser.

This report supersedes the earlier not-imported status in STATUS.md and terrain-manifest.json; those source-generation records remain unchanged pending the completed importer manifest.
