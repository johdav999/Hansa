# Hansa dirt road kit

Five editable, reference-grounded road surfaces, imported and saved in Hansa staging. A real three-segment Unreal spline example was saved, reloaded and checked. **Draft for review, not production/runtime acceptance.**

## Deliverables

- [Packed editable Blender master](exports/Hansa_DirtRoad_Kit.blend)
- Five individual FBXs and five self-contained GLBs under [exports](exports/).
- [Actual Unreal kit preview](renders/unreal_Kit.png), [native spline preview](renders/unreal_Spline.png), [junction close-up](renders/unreal_Junction.png).
- [Blender kit](renders/v4_Kit.jpg), [surface close-up](renders/v4_Surface.jpg), [raking light](renders/final_Raking.jpg), [24-view turntable](renders/DirtRoad_Turntable.mp4).
- [Reference/render comparison](COMPARISON.html), [material gap ledger](MATERIAL_ASSESSMENT.md), [evaluation](EVALUATION.md), [provenance](PROVENANCE.md), [iterations](ITERATIONS.md), [reference manifest](reference_manifest.csv), [material inventory](material_inventory.json).
- Built-in ImageGen [final prompt and native source record](textures/dirt-road--basecolor--v1.prompt.md).

These are actual meshes and materials. Render images are review references, not shipping surfaces. No generated full-screen artwork is used in the game.

## Component inventory

| Mesh | Vertices | LOD0 triangles |
|---|---:|---:|
| SM_DirtRoad_Straight_8m | 3,969 | 7,680 |
| SM_DirtRoad_Corner90_R6m | 4,753 | 9,216 |
| SM_DirtRoad_End_5m | 3,969 | 7,680 |
| SM_DirtRoad_TJunction_12m | 7,779 | 15,148 |
| SM_DirtRoad_Crossroads_12m | 9,517 | 18,556 |

Straight: 8 m along X, nominal 4.8 m shoulder-to-shoulder width, 3.6 m approximate travelled width. Outer irregularities reach 4.9225 m maximum. End: 5 m long, narrowing and lowering into terrain. Corner: 90 degrees, 6 m centreline radius, 8.4 x 8.4 m local bounds. Crossroads: 12 x 12 m. T-junction: 12 x 8.5625 m. Dimensions are design estimates; the photograph provides no survey scale.

One shared material family: dry compacted sand/gravel. Shared crowned connection profile; 49 vertices across each straight connector, longitudinal subdivisions every 10 cm. Geometry carries shallow wheel depressions and shoulder falloff. Vertex colors carry restrained wear and shoulder tint. No UI controls, icons or interactive artwork; UI component states are not applicable.

## Unreal location

Project: `C:/Users/Johan/source/repos/Unreal/Hansa/Hansa/Hansa.uproject`

Staging root: `/Game/Hansa/Generated/Staging/HansaDirtRoad_20260906_01/`

Preview level: `/Game/Hansa/Developer/GenerationPreview/HansaDirtRoad_20260906_01/L_DirtRoad_Review`

Example: `/Game/Hansa/Generated/Staging/HansaDirtRoad_20260906_01/BP_DirtRoad_SplineExample`

Future production destination, after explicit approval and final QA: `/Game/Mesh/hansa-dirt-road/`. No promotion occurred. No gameplay map, road definition, schema, or placement system was changed. Existing staging/developer NeverCook exclusions were confirmed in Config/DefaultGame.ini; a Shipping cook was not run.

## Use with splines

Use `SM_DirtRoad_Straight_8m` as the spline segment mesh. Set **Forward Axis = X**, up direction Z, start/end scale `(1,1)`, and identity relative transform. Keep the parent and segments at compatible mobility. The supplied example uses movable components attached to its DefaultSceneRoot and demonstrates horizontal bending plus a one-metre rise.

For each adjacent pair of SplineComponent points, get local position and tangent for each endpoint; call SplineMeshComponent `SetStartAndEnd`. Use the same end/start positions and tangents at joins. Do not force a single long mesh over an arbitrarily long spline: distribute roughly 6–8 m segments along arc length to keep grain density near its authored 2 m UV tile. Avoid very tight bends below approximately a 6 m radius and steep twists without new tests.

The saved Blueprint is a **fixed native verification example**, not a construction-script road editor. Editing its RoadPath does not automatically rebuild its mesh segments. For an editable gameplay road tool, implement the above loop in its construction/update function or assign the straight mesh to Landscape Splines. [Epic's Landscape Splines documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-splines-in-unreal-engine).

Keep the T/cross/end pieces as static junction/control-point pieces and connect splines to their ports. Do not deform an entire T or cross with one SplineMeshComponent. Unreal centimetre connector centres:

- Straight: `(-400,0,0)` and `(400,0,0)`.
- T: `(-600,0,0)`, `(600,0,0)`, `(0,-600,0)`; rotate the actor for other orientations.
- Cross: `(+/-600,0,0)`, `(0,+/-600,0)`.
- Corner: `(0,600,0)` and `(600,0,0)`; asset pivot is the arc centre.
- End: incoming `(-250,0,0)`; outgoing end is a lowered taper, not another modular socket.

Unreal import changes Y handedness relative to Blender: the source corner occupies +X/-Y, imported +X/+Y; the source T branch +Y becomes Unreal -Y. X length and Z elevation are preserved. All coordinates above refer to the imported mesh, not the rotated review actor.

These are thin terrain-overlay surfaces. Conform the landscape below the centre/crown and raise surrounding soil to cover the lowered shoulders; include vegetation separately. The isolated review intentionally exposes the perimeter and spline height to reveal geometry. It is not a finished landscape blend.

## Maps and verification

Built-in ImageGen base color: native 1254 x 1254, sRGB, physical tile 2 x 2 m (627 px/m). The tool returned this native size instead of requested 1024; it was preserved. Independently authored Blender grain normal and roughness: native 1024 x 1024, non-color (512 px/m). Blender tangent normals are +Y/OpenGL; Unreal imports with green flip. Unreal uses reduced normal XY scale 0.08 to limit micro-relief; source Blender strength is 0.5.

The source was reopened and all three consumed images verified packed. FBX and GLB were actually reimported into separate clean Blender processes; geometry, scale, UV/color layers and materials were checked and rendered. GLB retains material appearance. FBX needs the documented material reconstruction from image maps plus vertex colors, performed in the test and in Unreal.

Both native Unreal spline joins report **0.0 cm endpoint and tangent difference** after saved-level reload. Straight source connector profiles are identical with 49 vertices each. These measurements do not prove seamless texture phase across every rotated junction or arbitrary terrain deformation.

## Remaining limitations

- Unreal distant texture aliasing is still visible. The generated 1254-square base color is non-power-of-two; runtime mip/streaming optimization remains unresolved without an appropriate new native-size input or an explicitly approved alternative workflow. The source was not resized.
- Final terrain/grass blending is not included, and the lowered shoulder boundary is visible in an isolated preview.
- One LOD only; no performance, navigation, collision trace, or packaged Shipping test. Collision is configured as complex-as-simple and query/physics enabled on the spline sample, but configuration alone is not runtime proof.
- T/cross wheel wear is simplified around the shared centre. Repeat phase can show under some rotations. No wet, winter, or muddy variants.

Rebuild sources with installed Blender using `--background --factory-startup -t 6 --python-exit-code 1 --python scripts/build_road.py -- 4`. Intermediate corrections/logs remain in `Saved/GenerationJobs/hansa-dirt-road_20260906_01/`. The retained source package contains selected artifacts and evidence; checksums are in `manifest.json`.
