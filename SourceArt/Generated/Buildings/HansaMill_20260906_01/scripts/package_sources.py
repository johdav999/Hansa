from pathlib import Path
from PIL import Image
import json,hashlib,shutil,csv,html
P=Path(__file__).resolve().parents[1];root=P.parents[2];D=root/'SourceArt/Generated/Buildings/HansaMill_20260906_01';D.mkdir(parents=True,exist_ok=True)
for folder in ['exports','textures','references','scripts','renders']:
 (D/folder).mkdir(exist_ok=True)
 for f in (P/folder).iterdir():
  if not f.is_file():continue
  if f.suffix in ['.blend1','.pyc'] or f.name.startswith('schema_') or f.name.startswith('props_') or f.name.startswith('inputs_'):continue
  if folder=='exports' and (f.name.endswith('_BaseColor.png')):continue # superseded bakes stay in the job
  if folder=='scripts' and f.name in ['update_mip_textures.py']:continue
  shutil.copy2(f,D/folder/f.name)
for f in P.glob('*.json'):
 if f.name in ['unreal_mip_pending.json','unreal_assets_before_retry.json','unreal_seed.json','unreal_seed_nodes.json','unreal_classes.json']:continue
 shutil.copy2(f,D/f.name)
# FBX companion bitmap folder contains old copies too: keep only files referenced by final exports.
fbm=P/'exports/HansaMill.fbm'
if fbm.exists():
 (D/'exports/HansaMill.fbm').mkdir(exist_ok=True)
 for f in fbm.iterdir():
  if f.name.endswith('_BaseColor.png'):continue
  if f.is_file():shutil.copy2(f,D/'exports/HansaMill.fbm'/f.name)
for name in ['material_inventory.json']:
 text=(D/name).read_text();text=text.replace(str(P).replace('\\','\\\\'),str(D).replace('\\','\\\\'));(D/name).write_text(text)
# Store a few native-size turntable review frames; the complete sequence stays in the job.
for frame in [0,12,24,36]:
 f=P/'renders/turntable'/f'{frame:03}.png';shutil.copy2(f,D/'renders'/f'turntable_{frame:03}.png')
 with Image.open(f) as im:im.convert('RGB').save(P/'renders'/f'turntable_{frame:03}.jpg',quality=94,subsampling=0)
ref=P/'references/vilidu_oblique_Rutake_2014.jpg'
with (D/'reference_manifest.csv').open('w',newline='',encoding='utf8') as f:
 w=csv.writer(f);w.writerow(['reference_id','building','address_city','page_url','creator','capture_date_or_unknown','accessed_date','license','license_url','allowed_use','local_file','sha256','native_dimensions','view','observed_features','uncertainty'])
 w.writerow(['REF01','Vilidu post mill','Angla; Saaremaa; Estonia','https://commons.wikimedia.org/wiki/File:Angla_Vilidu_talu_pukktuulik_Saaremaal_august_2014_2.jpg','Rutake','2014 (filename/date metadata differ)','2026-09-06','CC BY-SA 3.0 Estonia','https://creativecommons.org/licenses/by-sa/3.0/ee/','Unmodified architectural research reference; not a shader input','references/'+ref.name,hashlib.sha256(ref.read_bytes()).hexdigest(),'1600x1276','Rear oblique','Vertical weatherboards; trestle mill on stone base; lattice sails; tailpole; rear openings','Restored later building; no measured dimensions; roof substrate and hidden details uncertain'])
 w.writerow(['REF02','Angla group','Saaremaa; Estonia','https://visitestonia.com/en/angla-windmill-mount','Visit Estonia','unknown','2026-09-06','Website text reference','', 'Historical context only','','','','Context','Early twentieth-century surviving mills, restored 2009–2011','Not a medieval reconstruction survey'])
 w.writerow(['REF03','Post-mill type','Buckinghamshire','https://heritageportal.buckinghamshire.gov.uk/theme/tbc834','Buckinghamshire heritage portal','unknown','2026-09-06','Website text reference','','Technology context only','','','','Context','Post mill, central post, tailpole; medieval documented examples','Not evidence for exact Baltic architecture'])
comparisons=[('Architectural reference / original Hansa interpretation','references/'+ref.name,'renders/r3_Rear.png'),('Gable / material correction','renders/r0_Hero.png','renders/r3_Hero.png'),('Foundation geometry correction','renders/r2_Base_Detail.png','renders/r3_Base_Detail.png'),('Packed-source material / clean GLB','renders/r3_Hero.png','renders/reimport_glb_Hero.png'),('Engine texture filtering correction','renders/unreal_before_exposure.png','renders/unreal_Hero.png'),('Source / engine timber and stone','renders/daylight_Base_Detail.png','renders/unreal_Base.png')]
parts=['<!doctype html><meta charset="utf-8"><title>Hansa Mill — native comparison evidence</title><style>body{font-family:system-ui;background:#202628;color:#f2e9d8;margin:24px}section{overflow:auto;margin-bottom:32px}.pair{display:flex;gap:16px;width:max-content}figure{margin:0}img{display:block;max-width:none;height:auto}figcaption{padding:8px}</style><h1>Hansa windmill comparison evidence</h1><p>Native source pixels. Scroll horizontally to compare; no image is rescaled. Reference and render cameras, image dimensions and lighting differ. This is an original adaptation, not a replica of the photographed later mill.</p>']
for label,a,b in comparisons:
 parts.append('<h2>'+html.escape(label)+'</h2><section><div class="pair">'+''.join('<figure><img src="'+x+'"><figcaption>'+html.escape(x)+'</figcaption></figure>' for x in [a,b])+'</div></section>')
(D/'COMPARISONS.html').write_text('\n'.join(parts),encoding='utf8')
manifest=[]
for f in D.rglob('*'):
 if f.is_file():manifest.append({'path':str(f.relative_to(D)),'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
(D/'manifest.json').write_text(json.dumps(manifest,indent=2));print('PACKAGE',D,'FILES',len(manifest))
