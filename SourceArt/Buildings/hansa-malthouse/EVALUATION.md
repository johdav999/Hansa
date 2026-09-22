# Evaluation — 2026-09-14

## Verified

- Four successive geometry/material correction cycles were rendered and inspected, followed by export and Unreal checks. See ITERATIONS.md.
- Delivered packed Blender master was reopened successfully. Every file-backed image was decoded and packed; source-reopen-audit.json records actual dimensions.
- Actual final FBX and GLB were reimported into clean geometry/material contexts. Both retained seven materials and dimensions within 5 mm of the source. Whole-building and doorway renders were produced; see clean-fbx/clean-glb JSON and PNG evidence.
- Imported through Unreal MCP into a new asset folder. All seven material slots explicitly assigned. All 21 imported textures read back as 1024²; normal compression/green flip and linear roughness configured. Materials compiled and packages saved.
- Review level saved and reopened, then its loaded model captured. Native scale 1 retained. Unreal uses centimetres and reverses the source Y handedness; +X remains the entrance.
- Bounds: 9.695 × 9.824 × 9.194 m. 153,379 vertices; 271,276 triangles. Fits the 11.6 m plot inset, ground contact approximately zero.

## Material and image evidence

Whole and detail Blender renders are native 1200 × 1000. Unreal viewport captures are native 2825 × 865. Turntable is 48 frames at 12 fps, 640², with inspected rear and hero stills. No source image resizing was used.

UV repeat extent is nominally 2.5 m. The 1024 bakes yield median area-equivalent density 409.60 px/m; fifth percentile 356.13 and minimum 17.13 on highly compressed/projected triangles. Density is not uniform: bevels, folds and sloped projections depart from nominal coverage. This does not establish the project's 384 px/m minimum on every triangle. High-magnification hero use would need targeted unwrap/atlas work. ImageGen color inputs decode at 1254², including reused oak.

Brick courses, tile seams, louvres and the kiln silhouette remain visible in clean reimports and standard Unreal rendering. Unreal lighting makes joints darker and more contrasty than the Blender studio reference. Small cloth weave and iron microdetail are not established by the whole-building capture. See material ledger and detail evidence; fine wear is restrained and not a claim of scanned photorealism.

## Remaining limits

- Historical reconstruction, with inferred kiln form and courtyard; no measured named-building fidelity claim.
- Nanite conversion produced missing/simplified small components and was disabled. Original geometry was recaptured intact. Conventional LODs and target-hardware profiling are still required before dense-city production use.
- Two simple collision boxes cover the hall and kiln, preserving the open workyard. Properties were saved/read back; runtime physics/navigation traversal was not tested.
- No gameplay asset mapping, animation, enterable interior, smoke, Shipping cook, or runtime approval in this job.

Status: created model and verified Unreal import for user review. Production promotion requires explicit approval under repository instructions.
