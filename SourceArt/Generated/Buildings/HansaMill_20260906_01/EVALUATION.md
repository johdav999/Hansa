# Evaluation

## Result

The delivered mill is an editable, materially weathered static 3D asset, with three built-in ImageGen color inputs actually integrated into source shaders and portable material maps. Whole-building renders, material close-ups, repeated swatches, clean format reimports, and actual reopened Unreal captures were inspected. Three explicit geometry/material correction cycles plus engine filtering and turntable-framing corrections are documented in [ITERATIONS.md](ITERATIONS.md).

The model achieves the requested worn/dirty Baltic windmill direction as an original game asset. It is not a scan-quality replica or a fully researched medieval reconstruction. Final visual review and production promotion remain the user's decision.

## Verified

- Real separate planks, roof shingles, framing, sail lattice, stones, stairs and iron details; no full-screen/generated image used as a 3D substitute.
- All three original color inputs are packed in the reopened Blender master.
- Native ImageGen dimensions 1254 × 1254 retained; each has an exact prompt record.
- Five portable PBR material sets; final engine maps all read back at 1024 × 1024 with data-map color-space settings.
- UV density measured from actual mesh triangles and physical dimensions, not inferred from texture size alone.
- Actual GLB and FBX exported and reimported into clean Blender scenes with scale checks and rendered evidence after the final bake change.
- Unreal import saved at the intended Hansa project's staging root, five exact material slots verified, bounds verified in centimetres including Y-axis handedness conversion, and the saved preview level reloaded before final captures.
- Alias-prone initial engine color maps replaced with new native shader bakes; generated originals unchanged.
- Turntable: corrected framing, 48 frames at 800 square, 12 fps, 4 seconds; cardinal views inspected.

## Limitations and production gates

Reference coverage is one useful native rear-oblique photograph plus historical context. Front, hidden joints, precise dimensions and period roof/hardware details are inferred. The mill is a parked static model with cloth removed. Functional machinery, animated sails, and interiors are not supplied.

Stonework remains smoother/more regularly coursed than the real reference. Very close inspection reveals simplified timber end grain and hardware/corrosion detail. This was designed for approximately 8–25 m game viewing, with material review around 2 m; it is not a macro architectural visualization asset.

Unreal reports 161,764 triangles and one LOD, versus 171,940 source/export triangles. The importer's triangle-count change is recorded rather than claimed lossless. Silhouette, bounds and visible component/material checks passed; per-triangle identity was not validated. Collision, navigation, distance LODs, draw-call/texture-memory budgets and Shipping exclusion/cook are not certified. A staging/developer path is not proof of Shipping exclusion.

Raw FBX retains maps and vertex-color data but requires a receiving material to multiply vertex color into base color; the delivered Unreal material does this. GLB supplies automatic portable color multiplication. See the [material gap ledger](MATERIAL_ASSESSMENT.md) for remaining visual approximations.
