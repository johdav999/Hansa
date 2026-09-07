from unreal_ops import Client,P
import json
c=Client();level='/Game/Hansa/Developer/GenerationPreview/HansaMill_20260906_01/L_MillPreview';cur=c.call('scene','get_current_level');assert not c.call('asset','is_dirty',{'asset_path':cur});assert not c.call('asset','exists',{'path':level})
assert c.call('asset','duplicate',{'path':'/Engine/Maps/Templates/Template_Default','new_path':level});c.call('scene','load_level',{'level_path':level})
actors=c.call('scene','find_actors',{'name':'','tag':'','collision_channels':[]});info={'level':level,'previous_level':cur}
for a in actors:
 if any(s in a['refPath'] for s in ['StaticMeshActor','Floor_0','PlayerStart']):c.call('scene','remove_from_scene',{'actor':a});continue
 comps=c.call('editor_toolset.toolsets.actor.ActorTools','get_components',{'actor':a})
 for co in comps:
  props=c.call('object','list_properties',{'instance':co});d=json.loads(props) if isinstance(props,str) else props
  if 'intensity' in d:
   kind='sun' if 'lightSourceAngle' in d else 'sky';info[kind]={'actor':a,'component':co};(P/'scripts'/('props_'+kind+'.json')).write_text(json.dumps(d,indent=2))
mesh=json.loads((P/'unreal_mesh.json').read_text());info['model']=c.call('scene','add_to_scene_from_asset',{'asset_path':mesh['refPath'],'name':'Hansa_Worn_Windmill','xform':{'location':{'x':0,'y':0,'z':5.34}}})
info['ground']=c.call('scene','add_to_scene_from_asset',{'asset_path':'/Engine/BasicShapes/Plane','name':'Mill_Review_Ground','xform':{'location':{'x':0,'y':0,'z':0},'scale':{'x':300,'y':300,'z':1}}})
(P/'preview_actors.json').write_text(json.dumps(info,indent=2));print(json.dumps(info))
