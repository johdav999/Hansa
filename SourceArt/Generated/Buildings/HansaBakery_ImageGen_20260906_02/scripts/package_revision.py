
from pathlib import Path
import json,shutil,hashlib,struct,zlib,html
B=Path.cwd().resolve();P=B/'Saved/GenerationJobs/hansa-bakery_imagegen_20260906_02';D=B/'SourceArt/Generated/Buildings/HansaBakery_ImageGen_20260906_02'
D.mkdir(exist_ok=False)
for n in ['textures','texture_sources','renders','evidence','scripts']:(D/n).mkdir()
# Move only final exports into the persistent source-art package, retaining exact file bytes.
for src in (P/'exports').iterdir():
 dest=D/src.name
 assert src.resolve().is_relative_to(B) and dest.resolve().is_relative_to(B)
 if src.is_file() and src.suffix not in ['.blend','.glb','.fbx','.mp4']:shutil.copy2(src,D/'evidence'/src.name);continue
 if src.is_dir():assert src.name=='HansaBakery.fbm'
 src.rename(dest)
inv=json.loads((P/'material_inventory.json').read_text())
for e in inv:
 for k,v in e['maps'].items():
  f=Path(v);shutil.copy2(f,D/'textures'/f.name);e['maps'][k]='textures/'+f.name
 if 'imagegen_source' in e:e['imagegen_source']='texture_sources/'+Path(e['imagegen_source']).name
(D/'evidence/material_inventory.json').write_text(json.dumps(inv,indent=2))
for n in ['texture_sources','renders','evidence']:
 for f in (P/n).iterdir():
  if f.is_file():shutil.copy2(f,D/n/f.name)
for f in (P/'scripts').glob('*.py'):shutil.copy2(f,D/'scripts'/f.name)
for n in ['reference_manifest.csv','unreal_import_final.json','unreal_materials.json']:
 shutil.copy2(P/n,D/('reference_manifest.csv' if n.endswith('.csv') else 'evidence/'+n))
(D/'README.md').write_text('''# Hansa Bakery — ImageGen material revision

Updated the existing bakery's brick, roof clay, lime plaster/damp-lime and oak using four built-in ImageGen surface-color masters across nine material variants. Structural geometry is unchanged.

- [Packed editable Blender master](HansaBakery.blend)
- [GLB](HansaBakery.glb) and [FBX](HansaBakery.fbx); keep HansaBakery.fbm beside the FBX.
- [Turntable](HansaBakery_turntable.mp4)
- [Blender preview](renders/final_Hero.png) and [native Unreal preview](renders/unreal_Hero.png)
- [Native-size comparisons](COMPARISONS.html)
- [Evaluation](EVALUATION.md), [material assessment](MATERIAL_ASSESSMENT.md), [iteration log](ITERATIONS.md), [provenance](PROVENANCE.md)
- [Material inventory](evidence/material_inventory.json), [prompt set](PROMPTS.md), [reference manifest](reference_manifest.csv)

In Blender, Portable_Export is the delivered mesh. HYBRID_ImageGen_* materials preserve editable generated-image/tint graphs and separate physical channels. Original authored pieces/procedural shaders remain in hidden collections for historical editing; those original materials are not the revised portable appearance. Reveal the original collections and object render visibility deliberately when editing geometry.

Unreal project: C:/Users/Johan/source/repos/Unreal/Hansa/Hansa/Hansa.uproject

Mesh: /Game/Hansa/Generated/Staging/HansaBakery_ImageGen_20260906_02/Meshes/SM_HansaBakery_ImageGen

Preview: /Game/Hansa/Developer/GenerationPreview/HansaBakery_ImageGen_20260906_02/L_BakeryPreview

Nine revised Unreal materials/color textures live under this revision's staging folder. Unchanged materials and physical-map dependencies retain references to the previous staging revision, which is preserved. No gameplay level, game definition or production asset was changed. Production promotion still requires explicit approval under repository AGENTS.md.

The original delivered package remains at ../HansaBakery_20260906_01/. Selected final exports were moved from Saved into this persistent folder to avoid duplicate disk use. Job scripts retain their execution-time Saved paths as evidence; final packaged model files are usable independently.
''')
(D/'PROVENANCE.md').write_text('''# Provenance

Creation mode: four built-in ImageGen generate calls, followed by headless Blender 3.5.1 shader integration, native-size color baking, and Unreal MCP import. No API/CLI image generation, purchased scans, photograph uploads or photo texture pixels were used.

Four generated masters are synthetic surface-color artwork, not measured albedo or photogrammetric scans. Each is preserved at the returned native 1254 x 1254 dimensions with its exact sibling prompt record. The prompt requested 1024 square if supported; the actual native 1254 result was accepted without resizing. Source masters remain unmodified.

Brick/clay sources cover 0.5 x 0.5 m; plaster/oak cover 2 x 2 m. Nine color variants were baked from editable image/tint shaders at matching 1254-square resolution and one-to-one UV coverage. Existing independently authored procedural roughness and normal maps remain 1024 square. No luminance-to-height or luminance-to-roughness conversion was used. Brick/clay UV coverage was updated for the finer physical source extent, and 180 timber pieces received deterministic UV phase offsets while preserving grain direction and all-channel registration.

Native Unreal textures initially imported with NoMipmaps. Enabling FromTextureGroup mip generation visibly reduced distance noise while keeping the 1254-square source data. This is ordinary engine mip generation, not resizing the source master. Original engine masonry normal adaptation is retained; the revised oak uses 10 percent sampled normal blended with 90 percent flat normal.

Historical architectural references and inferred bakery details are unchanged from the original package; see reference_manifest.csv and its original provenance. No new historical accuracy claim is made. The asset remains a staging draft.
''')
(D/'EVALUATION.md').write_text('''# Evaluation

Completed material revision, technically verified in source exports and Unreal staging.

- Four generated color masters; nine updated material variants; 17 total slots and 51 delivered PBR maps.
- Updated color maps: native 1254 x 1254. Physical channels and unchanged color maps: 1024 x 1024.
- Reopened Blender master: all 55 image datablocks packed (51 consumed maps plus four generated masters).
- Geometry remains 863,720 triangles / 496,234 vertices. Bounds match both clean reimports: approximately 10.007 x 19.485 x 16.645 m.
- Typical color density: plaster/oak 627 px/m; brick/clay 2508 px/m from the revised UV scale. Projection losses from the original mesh remain; these are nominal plane densities, not a claim that every sliver face achieves them.
- GLB and FBX each imported into separate clean Blender scenes with 51 images; hero, shop and roof renders inspected across formats.
- Final Unreal preview saved/reopened; all 17 slots and nine actual generated color connections verified. Source texture size/color space and mip settings read back.
- Native renders: Blender 1200 square; material swatches 1254 square; Unreal 2253 x 910. Turntable: 960 square, 96 frames, 24 fps.
- PNG signatures, dimensions, CRC and compressed data validated; selected deliverable hashes retained.

Visible limits: brick courses and roof tile layout remain regular; ImageGen improves surface color but cannot repair structural repetition. Oak knots repeat in a four-repeat flat swatch; coherent per-member offsets reduce matching phase on the actual model. Retained physical channels approximate the material rather than reconstruct the generated image's exact microscopic relief. Unreal daylight differs from Blender and some viewport/geometry aliasing remains. Glass, bread, sacks, metal and stone remain the original approximations.

Runtime gates are unchanged: no new LOD chain, custom collision, navigation, Shipping cook, budget acceptance or gameplay integration. This is a material revision in staging, not production approval or a claim of full photorealism.
''')
(D/'ITERATIONS.md').write_text('''# Material revision iterations

1. Inspected four native ImageGen outputs, then integrated them into nine image/tint color shaders. Rendered hero, shop, roof and four-repeat swatches. Plaster variation and irregular oak grain improve the previous procedural-only treatment; broad material swatches still reveal repeated oak knots.
2. Corrected repeated timber phase across 180 disconnected members, preserving vertical grain and common mapping across physical channels. Rerendered final views and verified the clean FBX shop / GLB hero results. Structural positions and topology remain unchanged.
3. Imported into an isolated Unreal preview. Read back all slot assignments and native generated color connections. Reduced retained oak normal weight to 10 percent for engine daylight; residual distance noise remained, so did not assume normals explained it.
4. Diagnosed NoMipmaps on all nine imported generated colors. Enabled FromTextureGroup mip generation without source padding/scaling; subsequent whole-building capture visibly reduced roof and timber noise. Captured final shop/roof views.
5. Packed-master check caught newly baked external image data; corrected packing, reopened and asserted 55 packed images, then rendered the full turntable and inspected rear frame 49. Clean GLB/FBX checks each found 51 maps and matching geometry bounds.

Temporary disk exhaustion was resolved by removing reproducible old clean-reimport scenes, automatic backups and byte-identical Saved export copies after verifying their persistent SourceArt originals. All original delivered artifacts remain preserved. This scoped material revision uses actual targeted corrections; it does not claim a new architectural modeling acceptance.
''')
(D/'MATERIAL_ASSESSMENT.md').write_text('''# Material gap ledger

| Family | Prior gap | Revision / evidence | Remaining limit |
|---|---|---|---|
| Lime plaster and damp lime | Regular procedural cloud variation | ImageGen fine coat/color detail in final_Detail_Shop.png and unreal_Shop.png | Period finish is inferred; physical bump retained independently |
| Brick, three variants | Uniform procedural faces | ImageGen fired-clay mineral color, physical 0.5m mapping; final_Hero.png and unreal_Hero.png | Brick shapes/courses remain regular |
| Roof clay, three variants | Flat/regular clay surface | ImageGen kiln/mineral detail and coherent whole-tile tint; final_Detail_Roof.png and unreal_Roof.png | Tile layout and tint repetition remain visible |
| Oak | Regular procedural grain | ImageGen irregular grain; 180 coherent member offsets; clean FBX shop render | Flat swatch still repeats knots; physical relief is approximate |
| Engine sampling | Generated color noise at distance | Found NoMipmaps; enabled FromTextureGroup, captured reduced noise | Viewport edge aliasing remains; runtime budgets unapproved |

Inspected all four generated originals, native repeated plaster/brick/oak swatches, actual building hero/shop/roof views, GLB hero and FBX shop reimport renders, Unreal hero/shop captures and turntable rear frame. Clay original and roof close-up establish its actual use. No photo or generated whole-screen mockup is shipped as a material. Native comparison panes retain original image dimensions.
''')
prompts=['# Final ImageGen prompt set\n']
for f in sorted((D/'texture_sources').glob('*.prompt.md')):prompts.append('## '+f.stem+'\n\n'+f.read_text())
(D/'PROMPTS.md').write_text('\n\n'.join(prompts))
panels=[('Previous Blender','renders/before_portable_Hero.png','Revised Blender','renders/final_Hero.png'),('Previous Unreal','renders/before_unreal_Hero.png','Revised Unreal','renders/unreal_Hero.png'),('Revised Blender shop','renders/final_Detail_Shop.png','Clean FBX shop','renders/reimport_fbx_Detail_Shop.png'),('Generated oak','texture_sources/bakery--oak--basecolor--1254x1254--v1.png','Revised Unreal shop','renders/unreal_Shop.png')]
body='<meta charset="utf-8"><title>Bakery material comparisons</title><style>body{font:16px system-ui;background:#eee;color:#222;margin:24px}.pair{display:flex;gap:16px}.pane{overflow:auto;max-width:48vw;max-height:75vh}img{max-width:none}h2{font-size:18px}</style><h1>Bakery material revision</h1><p>Native-size scroll panes. Cameras and lighting differ between Blender and Unreal. No raster resizing.</p>'
for a,ap,b,bp in panels:body+='<section class="pair"><div><h2>'+a+'</h2><div class="pane"><img src="'+ap+'"></div></div><div><h2>'+b+'</h2><div class="pane"><img src="'+bp+'"></div></div></section>'
(D/'COMPARISONS.html').write_text(body)
checks=[]
for f in D.rglob('*.png'):
 raw=f.read_bytes();assert raw[:8]==b'\x89PNG\r\n\x1a\n',f
 w,h=struct.unpack('>II',raw[16:24]);p=8;blocks=[]
 while p<len(raw):
  n=struct.unpack('>I',raw[p:p+4])[0];tag=raw[p+4:p+8];data=raw[p+8:p+8+n];crc=struct.unpack('>I',raw[p+8+n:p+12+n])[0];assert zlib.crc32(tag+data)&0xffffffff==crc,f
  if tag==b'IDAT':blocks.append(data)
  p+=12+n
 assert zlib.decompress(b''.join(blocks));checks.append({'file':str(f.relative_to(D)),'dimensions':[w,h],'integrity':'pass'})
(D/'evidence/png_validation.json').write_text(json.dumps(checks,indent=2))
baseline=json.loads((P/'evidence/baseline.json').read_text());assert hashlib.sha256(Path(baseline['source']).read_bytes()).hexdigest()==baseline['sha256']
(D/'MANIFEST.sha256.json').write_text(json.dumps({str(f.relative_to(D)):hashlib.sha256(f.read_bytes()).hexdigest() for f in D.rglob('*') if f.is_file() and f.name!='MANIFEST.sha256.json'},indent=2))
(P/'delivery.json').write_text(json.dumps({'persistent_package':str(D),'exports_moved':True},indent=2))
print('PACKAGE_VERIFIED',D,'PNG count',len(checks))

