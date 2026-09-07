from unreal_ops import Client,P
import json
c=Client()
print('mesh schema fields')
d=json.loads((P/'scripts'/'schema_mesh.decoded.json').read_text())
for t in d['tools']:
 if t['name'].split('.')[-1] in ['get_material_slots','set_material','get_bounds']:print(json.dumps(t))
print('texture output schemas')
d=json.loads((P/'scripts'/'schema_texture.decoded.json').read_text());print(json.dumps(d))
