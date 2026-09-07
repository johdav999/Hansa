# Animated grain-field patch — source package

Status: verified Blender/GLB source draft; Unreal import blocked by offline MCP. Not production-approved or runtime-optimized.

The crop uses a 4 x 4 m planting footprint, matching Hansa's 400 cm placement grid, with 1,600 mature wheat stalks. The soil is an independent flat mesh with no raised rim. Leaves overhang the planting footprint naturally: crop bounds are about 4.54 x 4.58 x 1.38 m. Place identical, unrotated tiles at 400 cm offsets with uniform scale 1.

## Files

- [Editable packed master](exports/GrainFieldPatch_Animated_Source.blend): original ImageGen inputs, procedural relief/roughness, geometry and animated shape keys.
- [Portable packed master](exports/GrainFieldPatch_Animated_Portable.blend): baked PBR maps and animated shape keys.
- [Animated GLB](exports/GrainFieldPatch_Animated.glb): one `GrainWind_8s_Loop` morph animation, with embedded textures.
- [FBX](exports/GrainFieldPatch_StaticForWindMaterial.fbx): geometry and materials; **no playback animation**. Intended for a future Unreal wind material.
- [Wind preview](renders/wind.mp4): 8 seconds, 24 fps, native 800 x 800.
- [Turntable](renders/turntable.mp4): 4 seconds, 24 fps, native 800 x 800.
- [Four neighbouring patches](renders/tiled_2x2.jpg): native 1200 x 1000.
- [Native-size comparisons](COMPARISONS.html), [evaluation](EVALUATION.md), [material gap ledger](MATERIAL_ASSESSMENT.md), [iterations](ITERATIONS.md), [provenance](PROVENANCE.md), [reference manifest](reference_manifest.csv), [material inventory](material_inventory.csv).
- [Export animation verification](glb_verification.json), [FBX verification](fbx_verification.json), [technical QA](technical_qa.json), [wind contract](wind_contract.json).

## Wind

Two morph targets produce a spatially phased, eight-second wind cycle. Root weight is `clamp(z / 1.35, 0, 1)^2`; the base remains fixed. Horizontal wind amplitudes are 11 cm and 3.5 cm. Spatial phase repeats across each 4 m tile, so adjacent copies at the same animation time have continuous wave phase. Use identical orientation and animation time. Arbitrary rotation or independent random time offsets are not seam-verified.

The GLB was exported again after a clean reimport exposed extra static animation clips. The final GLB has one merged wind clip and passes motion, root-lock and loop-closure checks. The source masters retain working Blender animation. Final morph targets use nonnegative 0–1 weights for viewer compatibility; all five quarter-cycle checkpoints were verified after clean GLB import. Pose equivalence to the rendered animation was checked before and after normalization.

## Unreal status and next step

Intended project: `C:/Users/Johan/source/repos/Unreal/Hansa/Hansa/Hansa.uproject` (UE 5.8). No Unreal assets or preview level were created. The local MCP endpoint at `http://127.0.0.1:8000/mcp` refused/timed out on repeated connection attempts. The normal desktop/node/image helper also failed during Windows sandbox initialization (`apply deny-read ACLs`); elevated read/build commands allowed source work to finish.

Open Hansa and run `ModelContextProtocol.StartServer 8000` in the Unreal console. That command was verified in the installed UE 5.8 plugin source. Then discover live tool schemas and confirm project identity before importing to `/Game/Hansa/Generated/Staging/HansaGrainFieldPatch_20260906_01/`. Implement and inspect the wind material, capture an isolated preview, and review the actual asset before production promotion. User-supplied AGENTS.md's explicit staging/promotion rule takes precedence over the installed skill's general `/Game/Mesh/` convention.

## Limits

363,200 crop triangles plus two soil triangles; four material families. This is a high-detail source asset. LODs, field-scale performance, wind bounds/culling, collision, navigation, terrain conformity, Shipping exclusion, and engine playback remain unverified. No gameplay definition or map was changed. Wheat heads and leaves are simplified; this is an artistic mature-wheat interpretation, not a surveyed medieval cultivar. Native 1254 x 1254 maps are non-power-of-two and need explicit Unreal mip/streaming review; no source raster was resized.

## Rebuild order

Run build.py with revisions 0 through 3 for review checkpoints, then finalize.py on checkpoint v3, then normalize_wind.py to save portable 0–1 morph targets and the final single-clip GLB. Run verify.py -- glb and -- fbx in clean Blender processes. movie.py renders wind and turntable previews. Original failed export diagnostics are retained as workflow evidence.
