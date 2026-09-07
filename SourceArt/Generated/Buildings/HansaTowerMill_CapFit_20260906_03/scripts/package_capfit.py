from pathlib import Path
import json,shutil,hashlib
from PIL import Image
P=Path(__file__).resolve().parents[1];repo=P.parents[2];old=P.parent/'hansa-tower-mill_20260906_02'
dest=repo/'SourceArt/Generated/Buildings/HansaTowerMill_CapFit_20260906_03'
prior=json.loads((P/'previous_master_hash.json').read_text(encoding='utf-8-sig'))
assert hashlib.sha256(Path(prior['Path']).read_bytes()).hexdigest().upper()==prior['Hash']
shutil.copytree(old/'textures',P/'textures',dirs_exist_ok=True)
for folder in ['exports','scripts','textures','references','renders']:
 shutil.copytree(P/folder,dest/folder,dirs_exist_ok=True,ignore=shutil.ignore_patterns('__pycache__','turntable','*.blend1'))
for f in P.glob('*.json'):shutil.copy2(f,dest/f.name)
geo=json.loads((P/'exports/geometry.json').read_text());ue=json.loads((P/'unreal_final_verification.json').read_text())
report=f'''# Tower windmill — corrected cap fit

The cap was centred, but its 2.15 m front/back half-depth was smaller than the 2.55 m tower crown radius. Its lower edge also sat 6 cm above the tower. This produced the apparent misalignment reported in the attached screenshot.

The corrected cap has a 2.90 m half-depth and unchanged 2.82 m half-width, remains centred at XY (0, 0), and sits 10 cm lower. A closed timber seating curb bridges the junction. Cap windows, sails and attached ironwork follow the revised front plane. Shingle UVs were adjusted to preserve their physical density.

## Deliverables

- Editable packed source: [HansaTowerMill_CapFit.blend](exports/HansaTowerMill_CapFit.blend).
- [FBX](exports/HansaTowerMill_CapFit.fbx) and [GLB](exports/HansaTowerMill_CapFit.glb).
- [Before/after comparison](comparison.html), [Unreal cap view](renders/unreal_CapFit.png), [Unreal side view](renders/unreal_CapSide.png), and [turntable](renders/turntable.mp4).
- New staging mesh: `/Game/Hansa/Generated/Staging/HansaTowerMill_20260906_02/Meshes/SM_HansaTowerMill_CapFit`.
- Updated saved preview: `/Game/Hansa/Developer/GenerationPreview/HansaTowerMill_20260906_02/L_TowerPreview`.

## Inventory and materials

Tapered masonry tower, brick-framed openings, sage doors, shingle cap and new seating curb, four lattice sails, shaft and iron fittings. Eight material families: masonry, recessed timber, fieldstone, weathered timber, brick, sage paint, glass and iron.

This geometry correction reuses the five original built-in ImageGen worn surface masters and their hybrid shaders. No new raster generation or resampling was needed. All five 1254 × 1254 originals and exact sibling prompt records are in [textures](textures/), and are packed in the source. The 24 portable shader-baked maps remain 1024 × 1024. Generation mode and exact prompt sets remain recorded beside each original. Glass, iron and hidden timber use the existing authored procedural exceptions.

## Verification

Clean FBX and GLB reimports passed bounds checks and were rendered. Source/export: {geo['triangles']:,} triangles; Unreal: {ue['triangles']:,}, one LOD. Import removes degenerate triangles. Unreal bounds, handedness, metre-to-centimetre conversion, eight material assignments, all texture dimensions and colour settings passed readback. Vertex colors use Replace, preserving weathering. The preview was saved and reopened successfully.

Native Blender cap comparisons and reimport renders are 1100 × 1100; Unreal captures are 1116 × 905. Side and overhead views were visually inspected: the crown is covered and the junction is seated. The 48-frame 800 × 800 turntable passed full-envelope framing checks. JPEG inspection copies retain their PNG dimensions.

The previous delivered source master was verified unchanged by SHA-256. See [fit measurements](fit_measurements.json), [Unreal checks](unreal_final_verification.json), and [import options](unreal_import_options.json).

## Scope

Imported assets remain review-stage content. Production promotion remains pending the explicit approval required by repository AGENTS.md. The preview images are evidence, not shipping textures. The original photo supports the front appearance; exact historical dimensions and rear construction remain inferred. This remains a static asset with opaque glass, without certified collision, navigation, animation, distance LODs, performance or Shipping-cook acceptance.
'''
(P/'README.md').write_text(report,encoding='utf-8');(dest/'README.md').write_text(report,encoding='utf-8')
html='<!doctype html><meta charset="utf-8"><title>Windmill cap fit correction</title><style>body{font:16px system-ui;background:#242424;color:#eee;margin:24px}img{display:block;margin:12px 0}a{color:#add8e6}</style><h1>Windmill cap fit correction</h1><p>Native-size evidence. Scroll to compare; no raster resizing.</p>'
for title,file in [('Before — crown exposed','before_Cap_Fit.png'),('After — crown covered','after_Cap_Fit.png'),('Corrected side junction','after_Cap_Side.png'),('Verified Unreal import','unreal_CapFit.png'),('Unreal side junction','unreal_CapSide.png')]:
 html+=f'<h2>{title}</h2><img src="renders/{file}" alt="{title}">'
html+='<p><a href="renders/turntable.mp4">Corrected turntable</a></p>'
(dest/'comparison.html').write_text(html,encoding='utf-8')
manifest=[]
for f in sorted(dest.rglob('*')):
 if f.is_file():manifest.append({'path':f.relative_to(dest).as_posix(),'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
(dest/'manifest.json').write_text(json.dumps(manifest,indent=2))
print(dest)
