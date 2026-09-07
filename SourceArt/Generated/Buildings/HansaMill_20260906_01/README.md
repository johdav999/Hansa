# Hansa — weathered Baltic windmill

Created an editable, textured post windmill and verified its staging import in Hansa. The material direction is worn and dirty, with desaturated weathered timber, aged shingles, sheltered grime, stone discoloration, and restrained oxidized iron. This is an original game interpretation informed by the Vilidu mill at Angla, not a measured replica or a proven medieval reconstruction.

## Deliverables

- [Packed editable Blender master](exports/HansaMill.blend) — named structure, cladding, roof, sails, access, stonework, hardware and review collections; original ImageGen inputs and procedural shaders retained.
- [GLB](exports/HansaMill.glb) — portable materials and vertex-color weathering.
- [FBX](exports/HansaMill.fbx) — standard PBR map references and vertex colors; receiving shaders must multiply vertex color into base color. Unreal reconstruction is verified.
- [Turntable](exports/HansaMill_turntable.mp4) — 48 freshly rendered 800 × 800 frames, 12 fps, 4 seconds; complete silhouette.
- [Native Unreal preview](renders/unreal_Hero.png) and [rear view](renders/unreal_Rear.png).
- [Native comparison evidence](COMPARISONS.html), [evaluation](EVALUATION.md), [iteration log](ITERATIONS.md), [material assessment](MATERIAL_ASSESSMENT.md), [provenance](PROVENANCE.md), [reference manifest](reference_manifest.csv), [material inventory](material_inventory.json), [file hashes](manifest.json).

## Component inventory

| Component | Construction / state |
|---|---|
| Foundation | Individually modeled fieldstones and recessed infill |
| Post and trestle | Central timber post, crossbeams and quarter bars |
| Mill body | Individual vertical planks, corner posts and floor/girts |
| Roof | Separate overlapping split shingles, bargeboards and ridge cap |
| Sail assembly | Four open lattice sails, leading boards, stocks, shaft and bindings; parked with cloth removed |
| Access | Framed rear door and upper hatch, open shutters, stair treads, landing and rails |
| Tailpole | Tailpole and lower support beam |
| Hardware | Timber pegs, iron hinges, shaft band and stock bindings |

All components are actual mesh geometry. No GUI is part of this asset; GUI state matrices are inapplicable. Sails remain separately editable in the source master. The delivered combined mesh is static.

## ImageGen and materials

Built-in ImageGen generated three original color masters, each **1254 × 1254**, opaque square, nominal 1 × 1 metre surface coverage. No API/CLI fallback or external paid model was used. The originals were not resized. Each has its full prompt and inspection record beside it:

- [Oak prompt](textures/mill--oak--worn--1254x1254--v1.prompt.md)
- [Roof timber prompt](textures/mill--roof--worn--1254x1254--v1.prompt.md)
- [Limestone prompt](textures/mill--stone--worn--1254x1254--v1.prompt.md)

These images feed actual source shaders. Roughness and submillimetre/millimetre relief are authored separately from procedural structure; stains are not converted into bumps. Broad weathering uses geometry-bound vertex colors. Sheltered oak is a darker material variant. Small forged-iron fittings use an independent procedural material because another color image adds little useful information there.

Final portable maps are native **1024 × 1024 shader bakes** (base color, roughness and OpenGL tangent normal for each of five materials). Color was rebaked natively from the original shaders to support Unreal mipmapping, replacing aliased 1254-square delivery bakes. This was not a resize of the generated masters. Color maps use sRGB; normal and roughness use linear/data settings. Unreal normal green-channel adaptation was explicitly configured and checked in close-ups.

## Scale and technical verification

- Inferred mill body: 3.6 × 4.2 m; eaves 6.5 m; roof ridge 8.2 m.
- Complete mesh envelope, including sails and tailpole: approximately **9.44 × 10.93 × 11.01 m**.
- Pivot is nominal ground centre. The irregular lowest stone extends 5.33 cm below nominal zero; preview placement compensates by 5.34 cm.
- Blender source/export geometry: 171,940 triangles; Unreal imported mesh readback: 161,764 triangles, five assigned slots, one LOD. The importer has remove-degenerates enabled and changes the triangle count; visual silhouette/material checks passed, but this is not a topology-equivalence claim.
- Area-weighted median delivery density: approximately 1024 px/m for wood/iron and 987 px/m for stone. Measured important-surface minima range from roughly 782–1024 px/m. See [density measurements](exports/texel_density.json); tiny slivers below 0.0001 m² are excluded from that summary.
- Packed master reopened and all three generated images verified packed: [master verification](exports/master_verification.json).
- GLB and FBX each imported into a clean Blender scene and rendered after the final bake update: [GLB](exports/reimport_glb.json), [FBX](exports/reimport_fbx.json).
- Image files decoded and dimensions/checksums checked: [image integrity](image_integrity.json).
- Unreal packages saved; preview level reopened; five material assignments, all final map dimensions, unit conversion and handedness verified: [engine verification](unreal_final_verification.json).

## Hansa paths and approval status

Project: `C:/Users/Johan/source/repos/Unreal/Hansa/Hansa/Hansa.uproject`

Staging mesh: `/Game/Hansa/Generated/Staging/HansaMill_20260906_01/Meshes/SM_HansaMill_Weathered`

Staging materials/textures: `/Game/Hansa/Generated/Staging/HansaMill_20260906_01/Materials/` and `/Textures/`.

Saved preview level: `/Game/Hansa/Developer/GenerationPreview/HansaMill_20260906_01/L_MillPreview`

Requested workflow destination after approval: `/Game/Mesh/hansa-mill/`. Repository AGENTS.md requires explicit approval before production promotion, so this deliverable remains in staging for review. No gameplay map reference was added.

## Remaining limits

This is a visually reviewed static asset, not runtime/Shipping acceptance. Interior milling machinery, cloth sails and rotation animation are not authored. Collision, navigation, distance LODs, memory/performance budgets, and a Shipping cook have not been validated. Architecture and proportions are inferred from later references; the shingle roof and hardware are artistic reconstructions. Stone shapes are somewhat smoother and more regularly coursed than the photographic reference. Extreme macro shots reveal simplified end grain, nails and wear geometry. Unreal daylight and neutral Blender lighting differ; compare each material in the supplied close-ups rather than treating color differences as a calibration measurement.
