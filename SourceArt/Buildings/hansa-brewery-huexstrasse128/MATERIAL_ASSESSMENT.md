# Production material and engine assessment

This ledger supplements the archived Blender R0–R5 comparison and photographic reference manifest. Final native screenshots use the saved production mesh in the isolated review level, with bright neutral lighting. They are render evidence, not generated reference artwork.

| Family / view | Observed engine gap | Correction | Evidence / remaining limits |
| --- | --- | --- | --- |
| Whole building | Reduced/cold Nanite views dropped window frames, roof tiles, and small props. Section indices and bounds were audited and were correct. A combined FBX alone did not resolve the symptom. | Preserved full mesh geometry and explicit tangents. Final production asset uses conventional static-mesh rendering; Nanite is disabled. | About 182k triangles and 303k vertices. Full-city performance and further LOD optimization remain unmeasured. |
| Terracotta roof | Imported color graphs omitted Blender's R05 tint layer, leaving pale tiles. | Reconstructed the exact R05 per-family RGB multipliers in Unreal, retaining generated base colors and independent physical maps. | Individual tile overlap and warm clay variation are visible in the native hero/roof review. Repeating tile-color order remains a visible stylization. |
| Broad brick walls | Bricks read too large; overly strong normal response produced diagonal striping. | Shared 4× UV repeat for wall color, roughness, and normal samples; tangent-normal XY strength 0.12, followed by normalization. | Native yard close-up confirms smaller brick repeats and substantially reduced striping. This is an approximate historical material, not a calibrated scan. |
| Facade brick / dark recess brick | Relief was stronger than needed at strategy distance. | XY normal strength 0.12 and normalization; retained facade geometry and existing UVs. | Gable piers and reveals carry depth. Facade regularity and clean condition remain approximations of the surviving building. |
| Oak | Physical-map relief was too prominent. | XY normal strength 0.12 and normalization; preserved ImageGen oak base color. | Small timber/cask features remain simplified; they are not intended for first-person inspection. |
| Copper, mash, burlap, iron, glass, stone | Broad material families read, but small dressing and glass are simplified. | Preserved approved separate materials and explicit slot assignments. | Kettle, casks, sacks, and rear/side work area are inferred gameplay reconstruction. No detailed interiors or physically accurate glass transmission are claimed. |

## Reproducible engine corrections

The original packed Blender master is preserved. These portable source/export limitations are corrected in Unreal by the retained job scripts: `export_combined.py`, `correct_material_fallback.py`, `correct_masonry_relief.py`, and `final_render_mode.py`. No base-color source image was resampled or replaced by procedural art.

Roof RGB tints: Terracotta (0.56, 0.29, 0.16), Dark (0.45, 0.23, 0.14), Warm (0.62, 0.31, 0.15), Smoked (0.43, 0.27, 0.20), Pale (0.60, 0.36, 0.23).

See [production promotion](PRODUCTION_PROMOTION.md) for the runtime path, test evidence, native captures, and remaining release checks.
