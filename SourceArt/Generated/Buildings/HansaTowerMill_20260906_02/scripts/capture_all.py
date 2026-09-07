from unreal_ops import Client,P
import json,subprocess,sys
c=Client();c.call('app','SelectActors',{'actors':[]});i=json.loads((P/'preview_actors.json').read_text());v=c.call('object','get_properties',{'instance':i['model'],'properties':['staticMeshComponent']});v=json.loads(v) if isinstance(v,str) else v;comp=v['staticMeshComponent'];c.call('object','list_properties',{'instance':comp});assert c.call('object','set_properties',{'instance':comp,'values':json.dumps({'relativeLocation':{'x':0,'y':0,'z':1.5}})});c.call('asset','save_assets',{'asset_paths':[i['level']]})
for view in ['Front','Hero','Rear','Base','Roof','Iron']:subprocess.run([sys.executable,str(P/'scripts/capture_unreal.py'),view],check=True)
from PIL import Image
for f in (P/'renders').glob('unreal_*.png'):
 with Image.open(f) as im:im.convert('RGB').save(f.with_suffix('.jpg'),quality=94,subsampling=0)
print('CAPTURES_COMPLETE')
