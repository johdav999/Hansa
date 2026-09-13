# World Partition and GeoReferencing setup

Date: 2026-09-07. Staged draft; not promoted.

- Enabled GeoReferencing in Hansa.uproject with explicit user approval. Its engine descriptor supports Win64 and separates runtime/editor modules.
- Converted only `/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/L_Lubeck_Terrain_Preview` using the native WorldPartitionConvertCommandlet with `-ConversionSuffix -AllowCommandletRendering -SCCProvider=None`.
- Output: `/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/L_Lubeck_Terrain_Preview_WP`.
- Original preview and gameplay map preserved.
- Conversion log reported `CONVERSION COMPLETED SUCCESSFULLY` and saved 31 packages.
- A subsequent fresh commandlet loaded the converted map and verified a native WorldPartition object on WorldSettings.
- Created and saved `GeoReference_Lubeck_EPSG25832` using native GeoReferencingSystem.
- Settings: FlatPlanet; projected CRS EPSG:25832; geographic CRS EPSG:4258; projected origin (610866, 5969930, 0) metres.
- Coordinate conversions checked with 0.001 m tolerance:
  - Unreal (0, 0, 0) cm → projected (610866, 5969930, 0) m.
  - Unreal (-201550, -201650, 1909) cm → projected (608850.5, 5971946.5, 19.09) m.
  - Unreal (201650, 201550, 999) cm → projected (612882.5, 5967914.5, 9.99) m.

## Remaining limitations

The running interactive editor predates plugin enablement and needs restart before opening the georeferenced WP map. No forced editor shutdown was performed.

Commandlet process exit was nonzero because of existing GameFeatureData asset-manager errors and MCP port 8000 already being occupied by the interactive editor. Conversion explicitly reported success; the subsequent script verified the partition and all three coordinate checks and logged `GEO CONFIGURED AND SAVED`. This is not a clean-project acceptance claim.

Materials, native water actors, survey-layer protection, full streamed collision/visual tests, source vertical-datum verification, performance, and cooking checks remain outstanding. Geographic CRS configuration does not establish or convert the source vertical datum. Earlier reports describing GeoReferencing and World Partition as unavailable are superseded by this report.
