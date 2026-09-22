# Terrain surface realism — 2026-09-17

## Scope and implementation

Requested first pass: broad grass/soil variation, broken road shoulders, connected entrance paths. This is native material and presentation work using existing raster inputs, not new generated artwork or a terrain-data import. The surveyed terrain, elevation, water geometry, lighting and simulation rules are unchanged.

- Terrain: existing grass-loam and bank-loam inputs retain their native texel scale. A world-space material mask blends meadow/dry grass and exposed loam at 24 m, 7.6 m and 2.2 m scales. This is an artistic ground-cover approximation, not a claim of measured medieval land cover. Original shoreline/layer inputs remain upstream.
- Road: world-space 73 cm and 19 cm variation erodes only the soft shoulder vertex coverage; the opaque travel lane and topology are preserved. Main-pass opacity and RVT writer opacity share the same mask. Metre-scale earth color variation helps the original fine dirt artwork read at gameplay distance; wetness is retained upstream.
- Compound entrances: paths now target within 60 cm of the road center rather than a cell corner 180 cm away. This reaches even an isolated/end tile's solid dirt core. Coverage includes the rounded end beyond the center rather than clipping there. Uses existing authored entrance/activity graphs, rotation and persistent parcel seeds. No invented doorway locations for building types without compound access graphs.

## Authoring

`Scripts/ConfigureRoadTerrain.ps1` runs the native `HansaRoadMaterial` commandlet. Its implementation is `Source/HansaEditor/Private/World/HansaGroundSurfaceAuthoring.inl`, included by the existing commandlet. It updates the existing graphs idempotently, before the RVT receiver blend, including the non-RVT fallback.

Material-instance controls: `GroundVariationStrength` (0.85), `GroundPatchScaleCm` (2400), `ShoulderBreakup` (0.7). Set strength/breakup to zero to disable those effects. No new gameplay data, schema, migration, generation-provider dependency or runtime editor dependency.

`Scripts/ApplyGroundRoadRefinement.py` can apply only the final road earth node through native Unreal Python, reading the exact HLSL from the C++ author rather than maintaining another shader. This was used when concurrent research-system edits temporarily blocked the whole-project build.

## Assets and provenance

Edited existing materials:

- `/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/Materials/M_Terrain_Hansa_Master`
- `/Game/Hansa/Generated/Staging/LubeckWorldArt_P30/M_Lubeck_Ground`
- `/Game/Mesh/hansa-dirt-road/Materials/M_Road_Terrain`

Existing source masters and prompt records remain in `SourceArt/Terrain/Lubeck/Materials/` and `SourceArt/Generated/Roads/`. No texture resampling, new raster generation or production promotion occurred. The terrain retains its existing staging status; this work does not claim Shipping readiness or approve its historical reconstruction. Road material changes introduce no staging reference.

Task-local backups of the three materials and the latest available autosave are in `Saved/TerrainSurface/Before/`. The editor had closed before a fresh live-session save could be requested; the autosave was backed up, not represented as a new manual save.

## Validation

- Editor build passed for the initial terrain/path implementation.
- `Hansa.World.CompoundGround`: 2 tests passed (terrain fit, determinism, ownership, cleanup, road-core connection and stable tie breaking).
- `Hansa.World.RoadTerrain`: 3 headless tests passed, including matching visible/RVT shoulder masks, valid shader inputs, and identical terrain variation in the RVT fallback.
- Real surveyed-map road captures exercise near/gameplay views, junction removal, save/load and RVT disabled.
- `Hansa.Compound.PlayerFlow` passed its isolated construction/upgrade/save/load/district flow. Its prototype map is useful for path checks but does not prove surveyed landscape appearance.
- Capture runner now rejects material compilation failures as well as failed test assertions.

Final visual verdict and build state recorded after the last render below.

### Final result

- Final complete Editor Development build: passed (`Saved/BuildArtifacts/20260917-183214590-build-HansaEditor-Win64-Development`). The temporary concurrent research compile failures were resolved before the successful final build.
- Compiled commandlet reapplied successfully, preserving the final material graph without nested nodes/cycles.
- Final road/terrain tests: 3 passed; compound-ground tests: 2 passed.
- Final real surveyed-map viewport test: passed at native 1920x1080; the same material refinement also passed at native 1280x720. No material compilation failures in the final capture logs.
- Inspected broad meadow/soil variation, warmer/darker road earth and irregular narrow shoulders. Road core continuity is preserved; no high-contrast camouflage pattern or new surface geometry was added.
- Native PNG evidence: [gameplay](TerrainSurfaceRealism/gameplay-1920x1080.png), [roads](TerrainSurfaceRealism/roads-1920x1080.png), [720p](TerrainSurfaceRealism/roads-1280x720.png), [RVT disabled](TerrainSurfaceRealism/rvt-disabled-1920x1080.png). JPEG transport previews used for inspection retained original dimensions; these saved evidence files are original PNG captures.
- Existing grass-loam and bank-loam masters are 1254x1254 pixels, unmodified. No new prompt set or generated raster asset applies to this native material revision.
- Full Shipping cook, wet-weather review, and all building-family doorway paths remain outside this bounded first pass. Existing staging terrain is not newly promoted. Entrance changes apply to compound parcels with authored access graphs.
