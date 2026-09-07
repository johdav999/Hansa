from unreal_ops import Client,P
import json
c=Client();info=json.loads((P/'preview_actors.json').read_text());sun=info['DirectionalLight_0_components'][0]
c.call('object','list_properties',{'instance':sun});c.call('object','set_properties',{'instance':sun,'values':json.dumps({'intensity':20000})})
pp=c.call('scene','add_to_scene_from_class',{'actor_type':{'refPath':'/Script/Engine.PostProcessVolume'},'name':'Bakery_Preview_Exposure','xform':{}})
props=c.call('object','list_properties',{'instance':pp});(P/'scripts'/'props_postprocess.json').write_text(props);info['postprocess']=pp;(P/'preview_actors.json').write_text(json.dumps(info,indent=2));d=json.loads(props)
print({k:v for k,v in d.items() if k in ['bUnbound','blendWeight','priority']})
print({k:v for k,v in d.get('settings',{}).get('properties',{}).items() if 'autoexposure' in k.lower()})
