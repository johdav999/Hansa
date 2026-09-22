# Daytime shadow and fill adjustment — 2026-09-16

User request: shorten scene shadows and brighten shaded building faces.

- Saved scene: L_Lubeck_Terrain_Preview_WP, DirectionalLight_0 elevation 60 degrees (was 49.517), retaining yaw, roll, location and scale; SkyLight_0 intensity 2 (was 1).
- Shared runtime lighting: daytime elevation lifted to at least 60 degrees, smoothly blending out between physical elevations 8 and 12 degrees; daylight sky fill 3.6 (was 1.8). Night fill, direct-light energy, exposure and source angle remain unchanged.
- Editor scene uses captured sky; gameplay uses the existing stable ambient cubemap, so their absolute sky intensities differ. Both have doubled fill.
- Rebuilt HansaEditor Development successfully. All six Hansa.World.LubeckArt automation tests passed.
- Reopened editor and confirmed saved scene values. PIE confirmed sun pitch -60 degrees and sky intensity 3.6. Inspected running scene; original screenshot camera was not reproduced. Stopped PIE after verification.
- No generated raster assets or GUI changes.