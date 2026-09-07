from unreal_ops import Client,P
import json
c=Client();level='/Game/Hansa/Developer/GenerationPreview/HansaTowerMill_20260906_02/L_TowerPreview'
c.call('scene','load_level',{'level_path':level})
actor={'refPath':level+'.L_TowerPreview:PersistentLevel.StaticMeshActor_1'}
c.call('object','list_properties',{'instance':actor});v=c.call('object','get_properties',{'instance':actor,'properties':['staticMeshComponent']});v=json.loads(v) if isinstance(v,str) else v;comp=v['staticMeshComponent']
c.call('object','list_properties',{'instance':comp});mesh=json.loads((P/'unreal_mesh.json').read_text());assert c.call('object','set_properties',{'instance':comp,'values':json.dumps({'staticMesh':mesh})})
assert c.call('asset','save_assets',{'asset_paths':[level]});(P/'preview_actors.json').write_text(json.dumps({'level':level,'model':actor},indent=2));print('PREVIEW_SAVED')
