from pathlib import Path
import shutil,json,hashlib,csv,zipfile
from PIL import Image
P=Path(__file__).resolve().parents[1];REPO=P.parents[2]
DEST=REPO/'SourceArt/Generated/Roads/HansaDirtRoad_20260906_01'
DEST.mkdir(parents=True,exist_ok=True)
def write(name,text): (P/name).write_text(text,encoding='utf8')
geo=json.loads((P/'evidence/geometry_v4.json').read_text())
inventory='\n'.join(f"| {name} | {r['vertices']:,} | {r['triangles']:,} |" for name,r in geo.items())
write('README.md',f'''# Hansa dirt road kit

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
{inventory}

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
''')
write('EVALUATION.md','''# Evaluation

Status: verified editable/exported assets and saved Unreal staging import; visual draft for review. Full photorealistic/runtime acceptance is not asserted.

Observed: five distinct silhouettes; gently crowned surface, shallow wheel depressions, irregular shoulders and a tapered low end. Built-in generated sand/gravel texture is integrated into actual source/export/Unreal shaders. Neutral and raking views, clean reimports, three turntable directions and actual Unreal captures inspected. Main source render corrections reduced excessive crown, dark shoulder bands and uniform bright wheel stripes.

Structural checks passed: five meshes with nonempty finite upward-facing polygons; no mesh validator repair required in final run; one material and UV0 per mesh; exact straight endpoint position profile; packed master reopened; five FBXs and GLBs cleanly reimported at intended metre scale; Unreal scale measured in centimetres; three actual spline mesh components persisted after reload; both endpoint and tangent joins measured zero difference.

Unreal-specific correction: static/movable attachment mismatch initially left spline surfaces at the world origin. All spline components were made movable and explicitly reparented in the Blueprint; refreshed instance transforms read back correctly. Material normal XY strength reduced to 0.08; distant aliasing remains chiefly in the native non-power-of-two base color. A closer neutral view reads as dry sand/gravel, but distant grain is harsher than the accepted Blender source.

No road gameplay integration or promotion. Terrain blending, image mip/streaming quality, LODs, collision traces/navigation, performance and final Shipping packaging remain open. Native input dimensions were preserved; no image resize was used to conceal the generator size limitation.
''')
write('ITERATIONS.md','''# Render / inspect / correct log

1. Source v1 inspected in v1_Kit.jpg and v1_Surface.jpg. Crown and narrow wear bands looked too sculpted versus the photograph. Reduced crown from 9 to 4.5 cm and rut depression from 5.5 to 2.5 cm; widened rut Gaussian. Rerendered v2; inspected v2_Surface.jpg and confirmed softer physical profile.
2. Dark shoulders remained too continuous and graphic. Reduced shoulder color attenuation from 43% to 24%, preserving geometry. Rerendered v3 and inspected v3_Kit.jpg; edges read as soil variation rather than dark borders.
3. Wheel wear still read as ruler-straight bright bands at kit distance. Added small endpoint-faded track wander and reduced brightening from 8% to 3.5%. Rerendered v4, inspected export/reimport and raking views; wear is restrained and less uniform.
4. Export correction: GLB tangent warning on quads. Added temporary export-only triangulation, regenerated both formats, reimported both in clean contexts and rendered kit/material views. Editable master retains quad topology.
5. Engine correction: first actual Unreal capture showed detached spline components and poor framing. Read component transforms, fixed incompatible mobility/reparenting in the saved Blueprint, refreshed only the isolated review actor. Reopened map, read component starts/ends and measured zero endpoint/tangent error. Improved camera and hid editor helpers for final captures.
6. Engine micro-relief reduced from XY 0.5 to 0.08. New actual capture inspected: some grain harshness remains, so mip/streaming visual acceptance stays open. No false claim that this correction eliminates the remaining non-power-of-two base-color aliasing.

Failed setup runs are not review cycles: Blender 3.5 UV-layer handle invalidation initially corrupted loop indices. Reacquiring UV layer after creating the color layer fixed it; final source uses validator assertions. Failed material wiring was inspected before resuming owned staging assets; no unrelated asset was overwritten.
''')
write('MATERIAL_ASSESSMENT.md','''# Material gap ledger

| Material / view | Reference observation | Render observation | Gap / cause | Correction | Evidence / status |
|---|---|---|---|---|---|
| Compacted dirt, source close-up | Fine sandy ground and small sparse gravel; warm sunset cast | Neutral dry ochre/beige mineral surface | Exact neutral albedo unknown from lit photo | Built-in synthetic base color, no photograph pixels; neutral light | v4_Surface.jpg, final_Raking.jpg; appearance interpretation |
| Road profile | Shallow irregular wear, broad soft ground | v1 crown and ruts overly pronounced | Height and narrow wear width | Half crown; broader, shallower wheel depressions | v1/v2_Surface.jpg; improved |
| Shoulder | Gradual irregular soil/grass transition | v2 dark edge too continuous | Vertex color attenuation | 43% to 24% attenuation | v3_Kit.jpg; improved; terrain blend remains open |
| Wheel wear | Tracks vary along road | Bright uniform parallel lines | Constant track position/brightening | Faded positional wander, weaker brightening | v4_Kit.jpg; improved |
| GLB / FBX | Same appearance target | GLB retains images and colors; FBX shader graph loses Multiply | Format shader limits | Documented map and vertex-color reconstruction for FBX | reimport_glb/fbx_Kit/Surface.jpg; checked |
| Unreal fine grain | Photograph is smoother at distance | High-frequency sparkle/noise at kit distance | Non-power-of-two base-color mip limitation; fine normals | Reduced normal XY scale to 0.08 | unreal_Kit.png; partial improvement, visual acceptance open |
| Modular edges / landscape | Road merges into grass/earth | Isolated surfaces expose boundaries, spline intentionally elevated | No receiving terrain blend | Lowered mesh shoulders and tapered end supplied; landscape integration still required | unreal_Spline.png; not production-complete |

Normals and roughness are synthetic physical channels, independent of pigment brightness; not measured scans. Source/image memory, LOD and package acceptance are separate from these visual observations.
''')
write('PROVENANCE.md','''# Provenance

User requested HansaModels dirt-road meshes with straight, corner, T, cross and end sections and spline usability. The attached photograph is the appearance anchor; descriptive content was not treated as additional user instructions. Source identity/location/date/licensing were not provided. It is retained privately as modeling reference, not embedded as production texture or represented as owned artwork.

Geometry: original parametric Python authored in this job; Blender 3.5.1 headless. Dimension estimates and neutral daylight color interpretation are design decisions, not measured road data.

Base color: one built-in ImageGen generate call. Tool model, seed, charge and version were not exposed. Original synthetic image native 1254 x 1254 preserved; prompt stored beside source. No paid provider API/CLI fallback used. Authoring uses material UV sampling; no raster resize performed. Generated texture is artwork, not scan or historical evidence.

Physical channels: original procedural grain shader baked in Blender at native 1024 x 1024; no luminance-to-height conversion. Normal convention +Y in Blender and green-flip on Unreal import. JPEG copies of PNG captures use the identical width and height for transport/inspection; original PNG captures are retained. Video uses native 1100 x 760 rendered frames, no spatial resampling.

Unreal: local MCP initialize/initialized and discovered toolsets, with task-bounded Python toolset registration using the installed engine's API. Exact Hansa project verified before writes. Imported only into new staging and developer preview folders. Source master/checkpoints and all other project work preserved. Production promotion not performed; explicit review/approval remains required by repository AGENTS.md.

Rights/commercial release of the reference photo are unknown. Remove the private photo from any public redistribution of this package. No external photograph or licensed scan was copied into delivered texture maps.
''')
material={'material':'M_DirtRoad','basecolor':{'file':'textures/dirt-road--basecolor--v1.png','origin':'built-in ImageGen synthetic','size':[1254,1254],'space':'sRGB','physical_tile_m':[2,2]},'normal':{'file':'textures/Dirt_Normal.png','origin':'independent Blender procedural grain bake','size':[1024,1024],'space':'linear','convention':'OpenGL +Y, flip green on Unreal import','source_strength':.5,'unreal_xy_scale':.08},'roughness':{'file':'textures/Dirt_Roughness.png','origin':'independent Blender procedural grain bake','size':[1024,1024],'space':'linear','authored_range':[.77,.95]},'vertex_colors':'RoadColor controls soil wear / shoulder tint, multiplied into base color','consumers':list(geo),'geometry':'crowning, rut relief, depressed shoulders; no shader displacement required','runtime_limitations':['NPOT base color distant aliasing','single LOD','terrain blend not included']}
write('material_inventory.json',json.dumps(material,indent=2))
ref=P/'references/user-road-reference.png'
with (P/'reference_manifest.csv').open('w',newline='',encoding='utf8') as f:
 w=csv.writer(f);w.writerow(['reference_id','kind','source','creator','date','license','allowed_use','local_file','sha256','native_dimensions','observations','uncertainty'])
 im=Image.open(ref);w.writerow(['R01','user-provided appearance reference','attachment','unknown','unknown','not supplied','private modeling reference only',str(ref.relative_to(P)),hashlib.sha256(ref.read_bytes()).hexdigest(),f'{im.width}x{im.height}','sandy road; gentle wear; soft irregular shoulders; sunset','dimensions, soil geology, location, date and neutral albedo unknown'])
write('COMPARISON.html','''<!doctype html><meta charset="utf-8"><title>Hansa dirt road reference comparison</title><style>body{font:16px system-ui;background:#202628;color:#F2E9D8;margin:24px}section{display:flex;gap:24px;align-items:flex-start}figure{margin:0}img{max-width:none}h1{font-size:25px}a{color:#C19A52}</style><h1>Dirt road — photographic reference and actual rendered assets</h1><p>Images retain their native pixel dimensions. Scroll horizontally to compare. The photo is sunset-lit; model renders use neutral review lighting. Perspective and framing differ; dimensions are estimated. Generated textures are not photographic evidence.</p><section><figure><figcaption>User photo: 1920 × 1521</figcaption><img src="references/user-road-reference.png"></figure><figure><figcaption>Actual Blender source: 1100 × 760</figcaption><img src="renders/v4_Surface.jpg"><figcaption>Actual Unreal spline: native viewport capture</figcaption><img src="renders/unreal_Spline.png"></figure></section><p><a href="MATERIAL_ASSESSMENT.md">Material gap ledger</a> · <a href="EVALUATION.md">Acceptance limits</a></p>''')
# Copy only selected delivery/evidence. Preserve failed/intermediate logs in Saved.
for folder in ['exports','textures','scripts','references','evidence']:
 (DEST/folder).mkdir(exist_ok=True)
 for f in (P/folder).iterdir():
  if f.is_file() and not f.name.endswith(('.blend1','.pyc')):shutil.copy2(f,DEST/folder/f.name)
(DEST/'renders').mkdir(exist_ok=True)
names=['v1_Kit.jpg','v1_Surface.jpg','v2_Surface.jpg','v3_Kit.jpg','v4_Kit.jpg','v4_Surface.jpg','v4_Junction.jpg','v4_Spline.jpg','final_Raking.jpg','DirtRoad_Turntable.mp4','unreal_Kit.png','unreal_Kit.jpg','unreal_Spline.png','unreal_Spline.jpg','unreal_Junction.png','unreal_Junction.jpg','unreal_Surface.png','reimport_glb_Kit.jpg','reimport_glb_Surface.jpg','reimport_fbx_Kit.jpg','reimport_fbx_Surface.jpg']
for name in names:shutil.copy2(P/'renders'/name,DEST/'renders'/name)
for f in P.iterdir():
 if f.suffix in ['.md','.csv','.json','.html']:shutil.copy2(f,DEST/f.name)
files=[]
for f in sorted(DEST.rglob('*')):
 if f.is_file() and f.name!='manifest.json':files.append({'path':f.relative_to(DEST).as_posix(),'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
(DEST/'manifest.json').write_text(json.dumps({'job':'hansa-dirt-road_20260906_01','status':'DraftReview','production_promoted':False,'files':files},indent=2))
print(json.dumps({'package':str(DEST),'files':len(files),'bytes':sum(f['bytes'] for f in files)}))
