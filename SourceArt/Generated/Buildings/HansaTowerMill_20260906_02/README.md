# Hansa tower windmill — reference revision

Rebuilt the earlier timber post mill as the user's pictured tapered masonry tower mill. The editable model has a hollow stone/plaster tower, three progressively smaller front windows, arched brick trim, open sage-green doors, a grey shingled cap, four long lattice sails, and a riveted diamond hub plate. The old model is preserved; its original master checksum is in [previous_master_hash.json](previous_master_hash.json).

## Deliverables

- [Packed editable Blender master](exports/HansaTowerMill.blend)
- [GLB](exports/HansaTowerMill.glb) and [FBX](exports/HansaTowerMill.fbx), with adjacent native PBR maps and FBX companion folder
- [800-square turntable](exports/HansaTowerMill_turntable.mp4): 48 frames, 12 fps, four seconds
- [Actual Unreal front preview](renders/unreal_Front.png) and [hero preview](renders/unreal_Hero.png)
- [Native side-by-side comparisons](COMPARISONS.html), [evaluation](EVALUATION.md), [iteration log](ITERATIONS.md), [material assessment](MATERIAL_ASSESSMENT.md), [provenance](PROVENANCE.md), [reference manifest](reference_manifest.csv), [material inventory](material_inventory.json), [image integrity](image_integrity.json), [hash manifest](manifest.json)

## Component inventory

| Family | Geometry / state |
|---|---|
| Tower | Tapered hollow masonry shell, actual arched reveals, partially exposed fieldstones |
| Windows | Three vertically aligned windows, fitted brick voussoirs/jambs, sills, sage frames and crossbars |
| Entrance | Open double plank doors, hinges/straps, threshold and simple recessed floor/post |
| Cap | Steep lower roof and short ridge roof, separate staggered shingles, front/back shingle cladding and cap windows |
| Sails | Four long open lattices, longitudinal spars, laths, leading windboards and stocks; parked, cloth absent |
| Hardware | Shaft, diamond plate, rivets, bindings and bolts |

This is a static mesh. Components remain separately editable in the source. No GUI was requested, so GUI interactive-state inventories do not apply.

## Materials and native dimensions

Four new built-in ImageGen color masters were generated for masonry/plaster, silver-grey timber, worn sage paint and old brick. One previously generated stone master is reused with provenance. All five originals are **1254 × 1254**, unchanged and packed in the source. The requested 1024-if-supported generation returned 1254-square images; no resize workaround was used. Each original has a sibling `.prompt.md` with the exact prompt and intended use under [textures](textures/).

The originals feed actual shaders. Independent procedural roughness and small physical relief are combined with geometry-bound weathering colors, including lower-wall dampness and sill runoff. Color-image brightness is not blindly converted to height. Iron, glass and hidden timber are authored procedural exceptions; glass uses a reflective opaque game approximation.

Eight materials each have native **1024 × 1024 shader-baked** base-color, roughness and OpenGL tangent-normal maps. These are fresh shader evaluations, not resized source rasters. Base color is sRGB; data maps are linear. Unreal uses normal compression and an explicit green-channel adaptation. Vertex colors are imported using **Replace** and multiplied into base color in every material.

## Scale and checks

Dimensions are inferred from the photograph, using a roughly 2 m doorway: tower base diameter 8 m, tower height 8.7 m, cap ridge 13 m. Complete envelope including sails is approximately **13.38 × 8.19 × 17.62 m**. Pivot is ground centre; preview offsets the lowest mesh point by 1.5 cm.

Source/export triangles: **283,648**. Unreal readback: **283,242**, 1 LOD, eight verified material slots. Import remove-degenerates is enabled; counts are recorded without claiming per-triangle equivalence.

Area-weighted median delivered UV density is about **410 px/m masonry**, **1024 px/m timber and paint**, **2048 px/m brick**, and **976 px/m fieldstone**. See [measured density](exports/texel_density.json) for ranges, including lower-density bevels and recessed surfaces. Native source detail remains finite; baking does not add photographed detail.

The packed master was reopened with five packed inputs verified. Final GLB and FBX were each imported into clean Blender scenes, measured and rendered. FBX requires receiving-shader vertex-color multiplication, applied explicitly in the verification scene and Unreal. The saved Unreal preview was reopened and all eight slots, map dimensions/color settings, bounds and import flags were read back. See [Unreal verification](unreal_final_verification.json) and [import settings](unreal_import_options.json).

## Unreal paths

Project: `C:/Users/Johan/source/repos/Unreal/Hansa/Hansa/Hansa.uproject`

Mesh: `/Game/Hansa/Generated/Staging/HansaTowerMill_20260906_02/Meshes/SM_HansaTowerMill`

Materials and textures: `/Game/Hansa/Generated/Staging/HansaTowerMill_20260906_02/Materials/` and `/Textures/`

Saved isolated preview: `/Game/Hansa/Developer/GenerationPreview/HansaTowerMill_20260906_02/L_TowerPreview`

Production promotion to `/Game/Mesh/hansa-tower-mill/` awaits explicit user approval required by repository AGENTS.md. No gameplay map or approved mesh was replaced.

## Limits

The front silhouette and material families follow the attached photograph; the exact building identity, measured dimensions, rear details and cap depth are unverified. The rear is an inferred continuation. Shingle wear and plaster damage are original synthetic interpretations, not an exact damage map. Small stone relief is simplified and some fine detail remains in the color input. Windows are opaque reflective panes, not a transmissive interior simulation. Full machinery, rotating sails, cloth, collision/navigation, distance LODs, performance budgets and Shipping cook acceptance are not delivered or certified.
