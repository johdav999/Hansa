from unreal_ops import Client,P
import json
c=Client();actor_ts='editor_toolset.toolsets.actor.ActorTools';level=c.call('scene','get_current_level');assert level.endswith('L_BakeryPreview')
actors=c.call('scene','find_actors',{'name':'','tag':'','collision_channels':[]})
for a in actors:
 if a['refPath'].split('.')[-1] in ['StaticMeshActor_0','Floor_0','PlayerStart_0'] or any(a['refPath'].endswith(':'+s) for s in []):c.call('scene','remove_from_scene',{'actor':a})
mesh=json.loads((P/'unreal_import.json').read_text())[0]
model=c.call('scene','add_to_scene_from_asset',{'asset_path':mesh['refPath'],'name':'Hansa_Bakery_Review','xform':{'location':{'x':0,'y':0,'z':-2}}})
ground=c.call('scene','add_to_scene_from_asset',{'asset_path':'/Engine/BasicShapes/Plane','name':'Bakery_Review_Ground','xform':{'location':{'x':0,'y':0,'z':-1},'scale':{'x':140,'y':140,'z':1}}})
info={'model':model,'ground':ground,'level':level}
for label,a in [('ground',ground)]+[(a['refPath'].split('.')[-1],a) for a in actors if any(s in a['refPath'] for s in ['DirectionalLight_0','SkyLight_0'])]:
 comps=c.call(actor_ts,'get_components',{'actor':a});info[label+'_components']=comps
 for j,co in enumerate(comps):
  props=c.call('object','list_properties',{'instance':co});(P/'scripts'/('props_'+label+'_'+str(j)+'.json')).write_text(props);print(label,co, list(json.loads(props).keys())[:12])
(P/'preview_actors.json').write_text(json.dumps(info,indent=2))
