from unreal_ops import Client,P
import json
c=Client();i=json.loads((P/'preview_actors.json').read_text());pp=c.call('scene','add_to_scene_from_class',{'actor_type':{'refPath':'/Script/Engine.PostProcessVolume'},'name':'Mill_Review_Exposure','xform':{}})
schema=c.call('object','list_properties',{'instance':pp});(P/'scripts/props_postprocess.json').write_text(schema if isinstance(schema,str) else json.dumps(schema));i['postprocess']=pp;(P/'preview_actors.json').write_text(json.dumps(i,indent=2));print('POSTPROCESS_CREATED',pp)
