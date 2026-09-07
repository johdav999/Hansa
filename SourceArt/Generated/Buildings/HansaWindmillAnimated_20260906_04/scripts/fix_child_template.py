from unreal_ops import Client,P
import json
c=Client();rec=json.loads((P/'unreal_animation.json').read_text());actor=c.call('scene','find_actors',{'tag':'HansaAnimatedWindmill','name':'','collision_channels':[]})[0];child=c.call('editor_toolset.toolsets.actor.ActorTools','get_components',{'actor':actor,'component_type':{'refPath':'/Script/Engine.ChildActorComponent'}})[0]
def get(ref,property):
 c.call('object','list_properties',{'instance':ref});d=c.call('object','get_properties',{'instance':ref,'properties':[property]});return (json.loads(d) if isinstance(d,str) else d)[property]
template=get(child,'childActorTemplate');component=get(template,'staticMeshComponent');c.call('object','list_properties',{'instance':component});assert c.call('object','set_properties',{'instance':component,'values':json.dumps({'staticMesh':rec['meshes']['Rotor']})});assert get(component,'staticMesh')==rec['meshes']['Rotor'];assert c.call('asset','save_assets',{'asset_paths':[rec['blueprint']]});print('CHILD_ARCHETYPE_CORRECTED',component)
