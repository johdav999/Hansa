from unreal_ops import Client,P
import json
c=Client();i=json.loads((P/'preview_actors.json').read_text())
c.call('scene','load_level',{'level_path':i['level']})
print(c.call('object','list_properties',{'instance':i['model']}))
print(c.call('object','get_properties',{'instance':i['model'],'properties':['staticMeshComponent']}))
