from pathlib import Path
import json,hashlib,shutil,csv,struct,zlib,datetime
ROOT=Path.cwd();J=ROOT/'Saved/GenerationJobs/hansa-bakery_20260906_01';D=ROOT/'SourceArt/Generated/Buildings/HansaBakery_20260906_01';D.mkdir(parents=True,exist_ok=False)
for f in ['HansaBakery.blend','HansaBakery.glb','HansaBakery.fbx','HansaBakery_turntable.mp4']:
 shutil.copy2(J/'exports'/f,D/f)
for sub in ['textures','scripts']:
 shutil.copytree(J/sub,D/sub)
shutil.copytree(J/'exports/HansaBakery.fbm',D/'HansaBakery.fbm')
(D/'renders').mkdir();(D/'evidence').mkdir()
selected=['portable_Hero.png','unreal_Hero.png','unreal_Shop.png','unreal_Roof.png','unreal_Rear.png','reimport_glb_Hero.png','reimport_glb_Detail_Roof.png','reimport_fbx_Hero.png','reimport_fbx_Detail_Shop.png','reimport_fbx_Detail_Roof.png','r0_hero.png','r1_hero.png','r2_hero.png','r3_Detail_Shop.png','r3_Rear.png','r5_Detail_Shop.png','r6_Detail_Roof.png','turntable_001.png','turntable_025.png','turntable_049.png','turntable_073.png','unreal_Hero_before_exposure.png']
for f in selected:shutil.copy2(J/'renders'/f,D/'renders'/f)
for f in ['mesh_manifest.json','master_reopen_check.json','measured_density.json','shader_cache_validation.json','reimport_glb.json','reimport_fbx.json']:
 shutil.copy2(J/'exports'/f,D/'evidence'/f)
for f in ['material_inventory.json','unreal_materials.json','unreal_import_final.json','preview_actors.json']:
 shutil.copy2(J/f,D/'evidence'/f)
# PNG CRC and image-stream verification without changing pixels.
image_results=[]
for f in list((D/'textures').glob('*.png'))+list((D/'renders').glob('*.png')):
 b=f.read_bytes();assert b[:8]==b'\x89PNG\r\n\x1a\n';w,h=struct.unpack('>II',b[16:24]);pos=8;compressed=[]
 while pos<len(b):
  n=struct.unpack('>I',b[pos:pos+4])[0];tag=b[pos+4:pos+8];data=b[pos+8:pos+8+n];crc=struct.unpack('>I',b[pos+8+n:pos+12+n])[0];assert zlib.crc32(tag+data)&0xffffffff==crc
  if tag==b'IDAT':compressed.append(data)
  pos+=12+n
 assert len(zlib.decompress(b''.join(compressed)))>w*h
 if f.parent.name=='textures':assert (w,h)==(1024,1024)
 image_results.append({'file':str(f.relative_to(D)),'width':w,'height':h,'crc_and_zlib':'pass'})
(D/'evidence/png_validation.json').write_text(json.dumps(image_results,indent=2))
model=json.loads((D/'evidence/mesh_manifest.json').read_text());density=json.loads((D/'evidence/measured_density.json').read_text())
refs=[{'reference_id':'R01','building':'Giebelhaus Muehlenstrasse 1','address_city':'Muehlenstrasse 1, Stralsund, Germany','page_url':'https://www.stralsundtourismus.de/en/poi/giebelhaus-muehlenstrasse-1','image_url':'https://dam.destination.one/3195984/56a03fd615d87e5983b9bda883bc9586e4a759f382a6f3d515eebdda43b24345/giebelhaus-m-hlenstrasse_1.jpg','creator':'TMV / Gaensicke (tourism-page credit)','capture_date_or_unknown':'unknown','accessed_date':'2026-09-06','license':'Not stated; reference-only, no texture pixel reuse','license_url':'unknown','allowed_use':'Internal architectural reference only; no redistribution license claimed','local_file':str(J/'references/muehlenstrasse1.jpg'),'sha256':hashlib.sha256((J/'references/muehlenstrasse1.jpg').read_bytes()).hexdigest(),'native_dimensions':'1575x2362','view':'front, oblique upward photographic view','observed_features':'concave brick gable wings; paired pointed recesses; projecting piers; pale lower plaster; roof-coping and metal pinnacles','uncertainty':'No survey dimensions or rear/roof photographic coverage; modern lower facade differs from modeled bakery'}, {'reference_id':'R02','building':'Giebelhaus Muehlenstrasse 1','address_city':'Muehlenstrasse 1, Stralsund, Germany','page_url':'https://www.ostsee.de/stralsund/altstadt-giebelhaus-muehlenstrasse1.php','image_url':'','creator':'ostsee.de','capture_date_or_unknown':'unknown','accessed_date':'2026-09-06','license':'Text research only','license_url':'','allowed_use':'Historical context, paraphrased','local_file':'','sha256':'','native_dimensions':'','view':'historical context','observed_features':'Documented former bakery and surviving oven; see authoritative tourism page for 17th-century oven','uncertainty':'Does not establish exact c.1650 street-front bakery arrangement'}]
with (D/'reference_manifest.csv').open('w',newline='',encoding='utf-8') as f:
 wr=csv.DictWriter(f,fieldnames=refs[0].keys());wr.writeheader();wr.writerows(refs)
(D/'PROVENANCE.md').write_text('''# Hansa bakery provenance

Created 2026-09-06 for the Hansa project using the user-requested hansamodels skill.

## Inputs and historical limits

R01 is a tourism photograph of Muehlenstrasse 1, Stralsund, credited TMV / Gaensicke. The official tourism page identifies a late-13th-century building remodeled in the 14th century and a surviving 17th-century oven. R02 supplies bakery-use context. URLs, photograph hash, observed features and limits are in reference_manifest.csv.

The model is an original exterior adaptation for Hansa, not a measured replica or a claim that the displayed shop existed in 1650. The main footprint (9 x 13 m), approximately 16.7 m maximum height, rear bakehouse, oven opening, chimney, trading hatches, shutters, sign, props, rear elevations and roof structure are inferred design decisions. The source photo depicts a restored modern condition, not a period survey. No unseen elevation was presented as photographed evidence.

## Creation mode and rights

Headless Blender 3.5.1, deterministic Python construction, procedural shaders and Cycles emission/normal baking. No ImageGen image, remote 3D provider, paid API, purchased asset or scanned texture was used. All shipping-candidate mesh geometry and texture pixels were constructed for this job. Photo pixels are not in the model or its maps. The reference photo remains in the internal job folder and is excluded from this selected asset package; no photo redistribution license is claimed.

Base color contains no baked lighting or AO. Roughness and tangent normals are separate. Portable glass is an opaque reflective approximation; original source shading remains editable. Bake samples are 2 x 2 m and all maps are natively 1024 x 1024. The geometry-only final revision reused shader-identical maps; shader and texture hashes are retained in evidence/shader_cache_validation.json.

## Approval boundary

Repository AGENTS.md requires generated drafts to enter staging and explicit approval before production promotion. This user-level instruction takes precedence over the skill's default /Game/Mesh destination. This package and the verified Unreal import are review drafts. No gameplay map or definition was changed. Intended post-approval destination: /Game/Mesh/hansa-bakery/.
''',encoding='utf-8')
(D/'PROMPTS.md').write_text('''# Final authoring prompt and build specification

User request: "use the HansaModel skill and create a realistic 3d model of a Hansa Bakery"

Generation mode: deterministic headless Blender Python. ImageGen mode: not used. Provider model: none. Blender version: 3.5.1. Seeds: 1701 for base construction, 78 for later masonry variation.

Final specification: create an editable, full exterior Hanseatic bakery inspired by the documented brick-gabled Muehlenstrasse 1 in Stralsund. Retain the tall paired gable openings, concave wings, projecting piers, pale lower plaster, muted brick and clay palette, and dark timber. Model structural depth, joints, overlapping clay tiles and solid ridge caps. Add an explicitly inferred early-modern bakery frontage, bread displays, a carved bread sign, rear bakehouse, oven opening, chimney and handling props. Use real metres, front -Y and up +Z. Preserve source collections, physically scaled procedural materials and portable maps. Avoid fantasy ornament, modern branding, copied game assets, baked dynamic text, photographic texture reuse, and provider dependencies.

Native image outputs: material maps 1024 x 1024; Blender review renders 1200 x 1200; turntable 960 x 960, 96 frames at 24 fps; Unreal viewport captures retain their native 2253 x 910 size. No raster resize was used to create delivery variants.

Final scripts: build_bakery.py with BAKERY_REV=3, refine_r4.py, refine_r5.py, refine_r6.py, then export_r6.py. The initial bake is bake_export.py against r5; export_r6.py consumes its shader-validated map cache. scripts were retained as executed evidence, with original job-relative paths. They are not an automated production importer.
''',encoding='utf-8')
(D/'ITERATIONS.md').write_text('''# Inspected iterations

All building comparison renders use fixed cameras, Filmic exposure and neutral daylight unless explicitly noted. Images are linked at native pixel dimensions.

| Cycle | Inspected defect | Implemented correction | Evidence and result |
|---|---|---|---|
| r0 to r1 | Roof was a solid sheet; front roof edge crossed upper openings; custom mesh winding inconsistent | Individual overlapping tiles, roof set back behind gable, recalculate custom winding | renders/r0_hero.png vs r1_hero.png: sheet and facade intersections corrected |
| r1 to r2 | Flat shop canopies, weak roof supports, overly coarse plaster and bright metal | Added canopy tile detail, corbels, sill drip courses; restrained plaster relief and metal response | renders/r1_hero.png vs r2_hero.png: added construction depth verified |
| r2 to r3 | Corners and doorway lacked close-range joint/hardware detail | Quoins, local plaster repairs and rivets | r3_Detail_Shop.png: details visible; exposed faceted bread and insufficient timber grain remained |
| r3 to r4/r5 | Rear windows faced inward; rear masonry/chimney and annex roof too plain; bread/sacks faceted | Reflected rear shutter assemblies, actual brick courses, annex tiles, organic smoothing, grain alignment, reduced plaster mottling, visible bread emblem and scoring | r3_Rear.png vs turntable_049.png; r3_Detail_Shop.png vs r5_Detail_Shop.png: construction and prop corrections visible |
| r5 to r6 | Engine roof close-up revealed ridge hoops; chimney cap obstructed flue | Closed, overlapping half-cylinder ridge shells and recessed open chimney throat | r6_Detail_Roof.png, reimport_glb_Detail_Roof.png, unreal_Roof.png and turntable_049.png: continuous ridge and open flue verified |
| Export UV correction | Measured median density was zero on several joined material families | Unified UV0_MetreTiling name, triangulation before per-face projection, fresh export | evidence/measured_density.json: no material has zero minimum density; median approximately 480-512 px/m |
| Unreal lighting | Native capture displayed cached-exposure warning and overly bright surfaces | Preview-local 12000-lux daylight, fixed EV100 11.5, neutral ground; save and reopen | unreal_Hero_before_exposure.png vs unreal_Hero.png: warning removed; no project-wide rendering configuration edited |

Final GLB and FBX were each reimported in clean Blender scenes after the UV and ridge changes. The final Blender master was reopened and all 51 consumed images were packed. The 96-frame turntable was regenerated from the final master. Native Unreal final mesh is the R2 staging revision; the earlier mesh remains only as a superseded staging comparison.
''',encoding='utf-8')
(D/'MATERIAL_ASSESSMENT.md').write_text('''# Material gap ledger

R01 is the architectural photograph. It is the reference for visible frontage material families, not proof of unseen rear construction. Comparison page: COMPARISONS.html. Original lighting and pixels are not altered. Cameras and framing differ between the photograph, Blender and Unreal.

| Family / view | Reference observation | Final render observation | Gap / uncertainty | Correction and final evidence |
|---|---|---|---|---|
| Brick / gable | Irregular fired-red masonry, lime joints, projecting mouldings | Individual bevelled bricks, recessed mortar, 3 restrained brick families | Courses and piers are more regular and simplified than photographed fabric | Recesses, winding and UVs repaired; portable_Hero.png, unreal_Hero.png. Adaptation, not conservation replica |
| Plaster / shop | Pale, relatively restrained lower render | Pale lime with restrained fine variation; small local repairs | Original period finish unknown; minimal age and damage treatment | Reduced bump and cloud contrast; r3_Detail_Shop.png to r5_Detail_Shop.png and reimport_fbx_Detail_Shop.png |
| Clay / main roof | Photo provides small coping details; full roof not visible | Separate overlapping clay shells, varied whole-tile colors and solid ridge shells | Full roof pattern inferred; repetition still visible at very close range | Replaced sheet and ridge hoops; reimport_glb_Detail_Roof.png and unreal_Roof.png |
| Stone / sills and foundation | Pale recess trim contrasts with brick | Shallow relief, bevelled blocks and explicit sills | Stone species and inferred foundation dimensions are not surveyed | Added sills and corner detailing; shop close-ups. No scan claim |
| Oak / door and shutters | Modern lower openings cannot establish historic timber detailing | Planked timber, directional grain, hinges, projecting shop shutters | Grain is procedural and comparatively regular, not a scanned aged surface | Grain reoriented, contrast restrained and boards separated; reimport_fbx_Detail_Shop.png, unreal_Shop.png |
| Iron and lead / fittings | Metal-tipped piers visible; shop hardware not documented | Matte hardware, controlled metallic response, lead-colored caps | Small hardware inferred; close-range patina simplified | Darkened cap response and added rivets; shop and gable review views |
| Glass / openings | Dark recessed glazing and pale reveals | Dark reflective panes with native mullions and lead lines | Portable opaque glass approximation; no full playable interior | Explicit portable and Unreal material handling; no default checker materials in inspected final views |
| Bread and linen / bakery | No source-photo evidence for these reconstructed props | Smooth bread forms, scoring and sacks establish bakery use | Props are inferred and retain simplified close-range surface detail | Replaced faceting; r3_Detail_Shop.png to r5_Detail_Shop.png |

Technical acceptance: geometry, scale, nonzero UV density, map dimensions, slot assignments, clean reimports and saved/reopened staging preview verified. Artistic status: reference-grounded exterior review draft, with regularized procedural surfaces; not a photogrammetric replica or an assertion of museum-grade photorealism. Human appearance approval and runtime optimization remain separate.
''',encoding='utf-8')
(D/'EVALUATION.md').write_text(f'''# Evaluation

Status: editable exterior model and technically verified Unreal staging import, awaiting human appearance review and production approval.

- Model: {model['triangles']:,} triangles and {model['vertices']:,} vertices after export preparation, 17 material slots.
- Measured overall bounds including props: {model['dimensions_m'][0]:.3f} x {model['dimensions_m'][1]:.3f} x {model['dimensions_m'][2]:.3f} metres. Main house footprint 9 x 13 m. Scale is inferred, not surveyed.
- Textures: 51 original procedural PNG maps, 1024 x 1024 each. CRC, compressed image streams and dimensions checked. All 51 consumed maps packed in the reopened master.
- UV coverage: one canonical UV0_MetreTiling channel. Median density about 480-512 px/m. Lowest measured values occur on small curved/boolean/sliver faces (approximately 252 px/m); detailed per-family measurements are in evidence/measured_density.json. No zero-density material minimum remains.
- GLB and FBX reimport: actual clean-scene imports, matching bounds to within floating-point tolerance, 51 maps present, rendered hero/roof/shop checks. Source unit is metres, front -Y, up +Z. Unreal correctly imports centimetres and reflects Y through its coordinate conversion; no extra manual scale factor was applied.
- Unreal: final SM_HansaBakery_R2 saved with all 17 named slots assigned. Every texture read back at 1024 x 1024. Base color is sRGB; roughness and normals are data; OpenGL +Y normals use Unreal green-channel inversion. Preview saved and reopened before captures.
- Preview: native 2253 x 910 engine screenshots, Blender stills 1200 x 1200. Turntable 960 x 960, 96 frames, 24 fps, four seconds. No raster resizing was used to create variants.

## Limits and remaining gates

The unseen rear, bakery shop arrangement and working props are reconstructions. The gable simplifies the photographed building's moulding and masonry irregularity. Materials remain procedurally regular and are intended for exterior game inspection around 5 m or farther, not macro photography. Glass is a documented opaque reflective approximation. Interior floors are a visual shell; no accessible interior or working oven/fire simulation is delivered.

This is a high-detail source asset. It has no authored LOD chain, custom collision, navigation test, production budget acceptance, gameplay integration, or Shipping cook proof. The substantial mesh/texture size needs a separate runtime optimization pass before widespread city placement. The preview and staging packages do not constitute production acceptance.

Windows sandbox process creation failed at the start; the approved shell fallback was used. Native image pixels were read through that shell because the image viewer and browser runtimes could not launch. This did not require changing the repository's render or application settings.
''',encoding='utf-8')
(D/'README.md').write_text('''# Hansa bakery — review package

An editable Hanseatic bakery exterior inspired by Muehlenstrasse 1 in Stralsund, with a reconstructed shop frontage and rear bakehouse. This is a technically verified staging draft awaiting appearance review and production approval.

## Open the model

- [Packed editable Blender master](HansaBakery.blend)
- [GLB](HansaBakery.glb)
- [FBX](HansaBakery.fbx) — keep the HansaBakery.fbm folder beside it
- [Turntable](HansaBakery_turntable.mp4)
- [Blender preview](renders/portable_Hero.png)
- [Native Unreal preview](renders/unreal_Hero.png)
- [Native material close-up](renders/unreal_Roof.png)

The master opens with Portable_Export visible. Original editable pieces and procedural shaders are retained in Structure, Masonry, Roof, Openings, Timber, Hardware and Bakery collections, hidden for the portable-material comparison. To edit the original, hide Portable_Export and reveal those source collections, including their object render visibility.

## Component inventory

- Hollow main structural shell and floor slabs; foundation and corner quoins.
- Brick gable, concave coping, projecting piers, metal-tipped pinnacles and paired lancets.
- Individually modelled overlapping clay roof tiles and solid ridge caps.
- Stone reveals, sills, glazed panes, mullions, door planks, shop shutters and supported canopies.
- Forged hinges, straps, door pull, rivets, projecting bread sign and chains.
- Rear bakehouse, brick chimney with open flue, oven opening, hearth, firewood and peel.
- Bread displays, carved bread emblem and flour sacks.
- Separate neutral lighting, cameras and review ground; not part of exported building geometry.

## Unreal locations

Project: C:/Users/Johan/source/repos/Unreal/Hansa/Hansa/Hansa.uproject, Unreal 5.8.

Final staging mesh: /Game/Hansa/Generated/Staging/HansaBakery_20260906_01/Meshes/SM_HansaBakery_R2

Materials and textures: /Game/Hansa/Generated/Staging/HansaBakery_20260906_01/Materials and /Textures.

Preview level: /Game/Hansa/Developer/GenerationPreview/HansaBakery_20260906_01/L_BakeryPreview.

The earlier SM_HansaBakery is a superseded staging comparison. No gameplay map or game definition was changed. Intended post-approval destination is /Game/Mesh/hansa-bakery/. Do not promote until explicit approval, per AGENTS.md.

## Evidence

- [Evaluation and limits](EVALUATION.md)
- [Provenance](PROVENANCE.md)
- [Reference manifest](reference_manifest.csv)
- [Inspection and correction history](ITERATIONS.md)
- [Material gap ledger](MATERIAL_ASSESSMENT.md)
- [Native-size side-by-side comparisons](COMPARISONS.html)
- [Material inventory](evidence/material_inventory.json)
- [Measured texel density](evidence/measured_density.json)
- [Authoring prompt and generation mode](PROMPTS.md)
- [File hashes](MANIFEST.sha256.json)

Detailed intermediate checkpoints, process logs and native original photograph remain in Saved/GenerationJobs/hansa-bakery_20260906_01/. The scripts in this package retain that original job-relative structure and are execution evidence. The final model files and their packed/sidecar textures are independently usable.
''',encoding='utf-8')
# Scrollable panes preserve original image pixel dimensions, no CSS scaling.
photo='../../../../Saved/GenerationJobs/hansa-bakery_20260906_01/references/muehlenstrasse1.jpg'
pairs=[('Photographic architectural reference / final Blender adaptation',photo,'renders/portable_Hero.png'),('Original blockout / corrected portable model','renders/r0_hero.png','renders/portable_Hero.png'),('Source roof / clean GLB reimport','renders/r6_Detail_Roof.png','renders/reimport_glb_Detail_Roof.png'),('Clean FBX shop / native Unreal shop','renders/reimport_fbx_Detail_Shop.png','renders/unreal_Shop.png'),('Portable Blender / native Unreal','renders/portable_Hero.png','renders/unreal_Hero.png')]
html='<!doctype html><meta charset="utf-8"><title>Hansa bakery comparisons</title><style>body{font:16px system-ui;background:#eee;color:#222;margin:24px}.pair{display:flex;gap:16px}.pane{flex:1;min-width:0;height:800px;overflow:auto;background:#bbb}img{max-width:none;max-height:none;width:auto;height:auto}h2{margin-top:32px}</style><h1>Hansa bakery: evidence comparisons</h1><p>Reference at left; model or later revision at right. Each pane scrolls at native image size. Photograph, camera angle, framing and output size differ; the photo is not recolored or resampled. The first image is architectural evidence, not a texture source. Unseen rear details are inferred.</p>'
for title,a,b in pairs:html+=f'<h2>{title}</h2><div class="pair"><div class="pane"><img src="{a}" alt="reference or earlier stage"></div><div class="pane"><img src="{b}" alt="current inspected model"></div></div>'
(D/'COMPARISONS.html').write_text(html,encoding='utf-8')
for name in ['README.md','PROVENANCE.md','PROMPTS.md','ITERATIONS.md','MATERIAL_ASSESSMENT.md','EVALUATION.md','reference_manifest.csv']:
 shutil.copy2(D/name,J/name)
manifest={str(f.relative_to(D)):hashlib.sha256(f.read_bytes()).hexdigest() for f in D.rglob('*') if f.is_file()};(D/'MANIFEST.sha256.json').write_text(json.dumps(manifest,indent=2))
print('DELIVERY',D);print('FILES',len(manifest));print('PNG_VALIDATED',len(image_results));print('BYTES',sum(f.stat().st_size for f in D.rglob('*') if f.is_file()))
