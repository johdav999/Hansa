from pathlib import Path
from PIL import Image
import shutil,json,hashlib,csv,html
P=Path(__file__).resolve().parents[1];D=P.parents[2]/'SourceArt/Generated/Buildings/HansaTowerMill_20260906_02';D.mkdir(parents=True,exist_ok=True)
for folder in ['exports','textures','references','renders','scripts']:
 (D/folder).mkdir(exist_ok=True)
 for f in (P/folder).iterdir():
  if f.is_file() and f.suffix not in ['.blend1','.pyc'] and not f.name.startswith('schema_') and f.name!='toolsets.json':shutil.copy2(f,D/folder/f.name)
fbm=P/'exports/HansaTowerMill.fbm'
if fbm.exists():shutil.copytree(fbm,D/'exports/HansaTowerMill.fbm',dirs_exist_ok=True)
for f in P.glob('*.json'):shutil.copy2(f,D/f.name)
for idx in [0,12,24,36]:
 f=P/'renders/turntable'/f'{idx:03}.png';shutil.copy2(f,D/'renders'/f'turntable_{idx:03}.png')
 with Image.open(f) as im:im.convert('RGB').save(D/'renders'/f'turntable_{idx:03}.jpg',quality=94,subsampling=0)
qa=[]
for folder in ['textures','exports','renders']:
 for f in (D/folder).glob('*.png'):
  with Image.open(f) as im:im.verify()
  with Image.open(f) as im:qa.append({'file':str(f.relative_to(D)),'dimensions':list(im.size),'mode':im.mode,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
(D/'image_integrity.json').write_text(json.dumps(qa,indent=2))
g=json.loads((P/'exports/geometry.json').read_text());ue=json.loads((P/'unreal_final_verification.json').read_text());density=json.loads((P/'exports/texel_density.json').read_text());ref=json.loads((P/'reference_manifest.json').read_text())
with (D/'reference_manifest.csv').open('w',newline='',encoding='utf-8') as f:
 w=csv.writer(f);w.writerow(['reference_id','type','source','creator_license','native_dimensions','sha256','observed','inferred'])
 w.writerow(['USER01','User supplied architecture appearance authority','references/user_reference.png','Unknown; supplied for modeling reference only',str(ref['dimensions']),ref['sha256'],ref['observed'],ref['inferred']])
 w.writerow(['CONTEXT01','Context only; identity unconfirmed','https://www.redzet.lv/en/travel/sights/windmills/araisi-windmill','Redzet / linked tourism information; no photograph pixels reused','','','Baltic stone tower mill with rotating cap','Not proof this is the attached building; no period or dimension claim'])
docs={
'README.md':f'''# Hansa tower windmill — reference revision

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

Source/export triangles: **{g['triangles']:,}**. Unreal readback: **{ue['triangles']:,}**, {ue['lod_count']} LOD, eight verified material slots. Import remove-degenerates is enabled; counts are recorded without claiming per-triangle equivalence.

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
''',
'ITERATIONS.md':'''# Actual correction history

1. **r0 → r1:** Inspected r0 Front against the user photograph. Arched cuts failed due to reversed cutter normals; tower used the cone's default UV layer, producing oversized masonry; timber was too pale. Corrected cutter normals, selected physical cylindrical UVs and darkened roof/sail condition. r1 Front confirms actual recesses and corrected stone scale.
2. **r1 → r2:** Inspected r1 Front. Box-shaped arch bricks created stepped tops and tower relief was too smooth. Replaced them with fitted wedge voussoirs and added partially exposed irregular stone geometry. r2 Front/Base confirms arch construction; Base also revealed excessive stone protrusion and clean brick.
3. **r2 → r3:** Inspected r2 Base. Recessed stones, reduced their color extremes, corrected their texture projection, generated an aged-brick color input and applied it, added localized sill runoff and lower-wall dampness, darkened timber and added small fasteners. r3 Front/Base/Roof confirms these changes. Three actual render-inspect-correct cycles completed.
4. **r3 → r4:** Roof close-up exposed aligned side-roof joints and a round hub cap unlike the reference. Staggered shingles and replaced the cap with a diamond iron plate and four rivets. r4 Roof confirms both.
5. **Delivery UV correction:** Actual triangle-based density measurement revealed near-zero projection on brick sides/reveals and competing UV channels. Reprojected side faces and normalized to one active UV channel. Re-exported and rerendered both real formats. Final density: masonry median ~410, brick ~2048, timber ~1024 px/m. Final GLB/Base and FBX/Base inspected.
6. **Unreal:** Imported eight explicit material sets with native 1024 maps and Replace vertex colors from the outset. Saved/reopened the separate preview, checked all assignments and bounds. Tightened camera framing, cleared selection overlays, softened directional shadow edges and corrected ground offset before final captures.

Blender's first r2 attempt crashed while evaluating new mesh custom data; refreshed UV-layer references after color-layer allocation and validated the new meshes, then reran successfully. These failed runs do not count as correction cycles. Unreal preview duplication initially held a Python world reference during load and triggered its world-cleanup check; the saved map was recovered through the standard scene loader in a fresh isolated editor. No source or staged asset was lost. Logs/checkpoints remain in the Saved job.
''',
'MATERIAL_ASSESSMENT.md':'''# Material comparison ledger

| Material / view | Reference observation | Initial gap | Correction and evidence | Remaining approximation |
|---|---|---|---|---|
| Masonry / front and base | Dirty grey-beige plaster with scattered exposed rubble | r0 oversized mapping; r1 too flat | Physical cylinder UVs, ImageGen plaster/rubble color, partial stone geometry and localized runoff; r3 Base and final reimports | Damage layout is not copied exactly; fine substrate relief is simplified |
| Stone / base | Stones mostly embedded in coat | r2 stones resembled attached lumps | Recessed 6.5 cm, muted hue extremes; r3 Base | Small exposed faces remain simplified polyhedra |
| Brick / openings | Worn red-brown arches and narrow jambs | Stepped box arch; overly clean surfaces | Fitted wedge bricks and new aged-brick ImageGen input; r3 Base and final FBX Base | Surrounds remain somewhat regular; no scanned erosion |
| Roof / cap | Grey staggered shingles and irregular edges | Aligned flank seams; pale timber | Staggered modeled shingles, restrained per-shingle tone, darkened weathering; r4 Roof | Cap depth/back silhouette inferred; edge wear simplified |
| Sail timber / roof close-up | Long grey stocks and thin lattice | Pale members; round hub cap | Grain-aligned ImageGen timber, darker condition, bindings/bolts, diamond iron plate; r4 Roof, engine Iron | No cloth or operational deformation |
| Sage paint / entrance | Faded green doors and frames | Flat clean coating | New worn-paint ImageGen source, separate physical relief; final Base | Paint flakes primarily color detail |
| Iron / hub | Dark plate and fasteners | Round cap silhouette | Diamond plate, rivets, rough oxidized metal; r4 Roof / engine Iron | Procedural rust/metal, no scanned corrosion |
| Glass / windows | Dark small panes with green frames | Transmission not established | Consistent reflective opaque approximation in source and engine | No true glass transmission or detailed interior |

Original inputs and repeated swatches were inspected at native size. Clean export renders preserve the visible color families and weathering. FBX's unsupported vertex-color material multiply is explicitly rebuilt, not assumed. Unreal uses separate material graphs and verified data-map settings. Source overcast and engine sun/sky lighting differ, so colors are compared as material families rather than calibrated albedo measurements. Intended use is approximately 5–30 m; very close shots expose the stated simplifications.
''',
'EVALUATION.md':'''# Evaluation

The revised asset follows the attached tower-windmill reference instead of the earlier wooden post mill: tapered round masonry, three vertically aligned windows, open green doors, grey shingled cap and four long lattice sails. Four visual geometry/material corrections plus an export UV correction were executed and inspected. This is an original editable interpretation, not a measured replica or photogrammetric reconstruction.

Verified: five packed ImageGen source images; eight independent portable PBR material sets; real clean GLB and FBX reimports; measured metre bounds and UV density; 48-frame native 800-square turntable with every bounding corner inside a 3% frame margin; actual Unreal staging import, saved/reopened preview, eight slots, map sizes, color spaces and Replace vertex colors. Final engine front/hero and material close-ups are retained for review.

This is visual asset review, not runtime acceptance. The mesh is detailed and has one imported LOD; optimization, collision/navigation, animation, machinery, memory budgets, package dependencies and Shipping exclusion/cook remain unvalidated. Production promotion is pending user approval. Material-specific visual limitations are recorded in MATERIAL_ASSESSMENT.md.
''',
'PROVENANCE.md':'''# Provenance

The user explicitly requested a rebuild from the attached photograph using HansaModels and ImageGen worn/dirty shader textures. The photograph is preserved unaltered in references with its original watermark, native dimensions and checksum. Photographer, exact building identity, date and license are unknown. It is an architectural reference only; no photograph pixels were used in shaders or sent to ImageGen. No ownership or production texture license is asserted for that image.

Four new color sources were generated with built-in ImageGen in this conversation: masonry/plaster, grey timber, sage paint and old brick. A fifth ImageGen stone input from the previous windmill job is reused. Exact prompts are beside the native originals; generator model version/seed and monetary usage were not exposed and are not invented. No API fallback, provider credentials, purchased asset, external 3D model, or paid service was used.

All geometry, UVs, masks, procedural channels and Blender scripts were authored for this task. Native sources are 1254-square; portable maps are new native 1024 shader bakes, not resized images. Normal/roughness detail is authored independently of image luminance. Vertex colors encode per-member condition, base dampness and sill runoff. PNGs are authoritative; JPEG files are same-dimension transport/inspection encodings. No image was geometrically resampled. Turntable frames were rendered directly at 800 square.

The old source master was preserved and hashed. New Unreal packages are confined to the unique staging root and developer preview level. Existing gameplay assets were not replaced. AGENTS.md requires explicit approval for promotion; the final runtime destination remains pending. Reference-context research is recorded separately and is not treated as identification evidence.
'''}
for name,text in docs.items():(D/name).write_text(text,encoding='utf-8');(P/name).write_text(text,encoding='utf-8')
pairs=[('User reference / rebuilt front','references/user_reference.png','renders/reimport_glb_Front.png'),('Opening and UV correction','renders/r0_Front.png','renders/r1_Front.png'),('Stone and brick correction','renders/r2_Base_Detail.png','renders/r3_Base_Detail.png'),('Final format comparison','renders/reimport_glb_Base_Detail.png','renders/reimport_fbx_Base_Detail.png'),('Source / Unreal front','renders/reimport_glb_Front.png','renders/unreal_Front.png'),('Source / Unreal materials','renders/reimport_glb_Base_Detail.png','renders/unreal_Base.png')]
parts=['<!doctype html><meta charset="utf-8"><title>Hansa tower mill comparisons</title><style>body{font-family:system-ui;margin:24px;background:#222;color:#eee}section{overflow:auto;margin:24px 0}.pair{display:flex;gap:20px;width:max-content}figure{margin:0}img{display:block;max-width:none;height:auto}figcaption{padding:8px}</style><h1>Native image comparisons</h1><p>Each image keeps its original pixels. Scroll horizontally; images are not rescaled. Reference and rendered camera/lighting differ. The user photograph is appearance evidence, not a measured survey.</p>']
for label,a,b in pairs:parts.append('<h2>'+label+'</h2><section><div class="pair">'+''.join('<figure><img src="'+x+'"><figcaption>'+html.escape(x)+'</figcaption></figure>' for x in [a,b])+'</div></section>')
(D/'COMPARISONS.html').write_text('\n'.join(parts),encoding='utf-8')
for f in (D/'textures').glob('*.prompt.md'):
 with f.open('a',encoding='utf-8') as out:out.write('\n\nFinal integration: native source retained and packed, actual shader and portable map consumers verified through clean reimport and Unreal. See material inventory and assessment.\n')
inventory=json.loads((D/'material_inventory.json').read_text())
for r in inventory:
 r['maps']={k:'exports/'+Path(v).name for k,v in r['maps'].items()};r['source']=r['source'].replace(str(P),'.')
(D/'material_inventory.json').write_text(json.dumps(inventory,indent=2))
manifest=[{'file':str(f.relative_to(D)),'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()} for f in D.rglob('*') if f.is_file() and f.name!='manifest.json'];(D/'manifest.json').write_text(json.dumps(manifest,indent=2));print('PACKAGED',str(D),len(manifest))
