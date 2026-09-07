from unreal_ops import Client,P
import json
c=Client();i=json.loads((P/'preview_actors.json').read_text());mesh=json.loads((P/'unreal_mesh.json').read_text())
d=c.call('object','get_properties',{'instance':i['model'],'properties':['staticMeshComponent']});d=json.loads(d) if isinstance(d,str) else d;comp=d['staticMeshComponent']
s=c.call('object','list_properties',{'instance':comp});s=json.loads(s) if isinstance(s,str) else s
assert 'staticMesh' in s
print(s['staticMesh'])
print(c.call('object','set_properties',{'instance':comp,'values':json.dumps({'staticMesh':mesh})}))
print(c.call('asset','save_assets',{'asset_paths':[i['level'],mesh['refPath']]}))
