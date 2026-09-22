# Lubeck blue water correction - 2026-09-16

The active surveyed map's River and Lake material instances had olive scattering
(0.35, 0.42, 0.25; strength 0.5), olive albedo (0.22, 0.25, 0.15), and nearly
neutral absorption distances (70, 95, 55; multiplier 8). The surface was present;
these settings left shallow water dominated by the brown terrain underneath.
At sample (-36000, 4000), runtime ground collision was Z=72.995 cm while the
river spline surface was Z=90 cm (approximately 17 cm of water).

Changed only the three optical vector parameters on both existing materials.
Blue-biased scattering and shorter absorption distances retain a readable blue
surface in shallow channels. Values are art direction, not measured chemistry.
Native Water meshes, reflections, normals, roughness, water levels, channel
geometry and gameplay data are retained. No generated imagery or new textures.

- Source settings: `SourceArt/Terrain/Lubeck/Survey_20260907/water-optics.json`.
- Reapply/verify: `Scripts/ApplyLubeckWaterOptics.py` (default read-only; `--apply` writes).
- Saved assets: `Content/Hansa/Generated/Staging/LubeckTerrain_20260907/Materials/MI_Water_Lubeck_River.uasset` and `MI_Water_Lubeck_Lake.uasset`.
- Native gameplay capture: `Docs/Images/Water/lubeck-water-optics-20260916.png`, 2825 x 865, no resampling.
- Pre-change backups and diagnostics: `Saved/WaterFix_20260916/`.

Validation: inspected the running assembled game at original screenshot resolution;
blue river surface is visible beside the user's existing houses and road. Both
material parameter sets passed readback in the current editor and a separate
Unreal commandlet loading the saved packages from disk. The commandlet emitted
`WATER_OPTICS_VERIFY_PASS`; its overall exit was 1 due to pre-existing missing
GameFeatureData asset-manager configuration and the second MCP listener attempting
to bind the occupied port 8000. This is not a clean whole-project test pass.
Temporary GPU-quadtree, wireframe, force-refresh and opacity-mask diagnostics were
restored to their original values. No running game restart or save replacement.

Limits: lake parameters were verified, but no separate runtime lake screenshot was
taken. Existing shallow/jagged shoreline geometry remains. Staged terrain assets
are ignored by repository policy; the checked-in candidate source settings and
repair script reproduce the local asset fix. No production promotion or packaging
claim is made. This supersedes the old transient 20260907 olive tuning values.
