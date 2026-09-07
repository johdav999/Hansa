from unreal_ops import Client,P
import json
c=Client();root='/Game/Hansa/Generated/Staging/HansaBakery_20260906_01';info=json.loads((P/'preview_actors.json').read_text())
assert c.call('scene','get_current_level')==info['level']
name='SM_HansaBakery_R2';assert not c.call('asset','exists',{'path':root+'/Meshes/'+name})
mesh=c.call('mesh','import_file',{'folder_path':root+'/Meshes','asset_name':name,'source_file':str(P/'exports'/'HansaBakery.fbx'),'combine_meshes':True,'import_materials':False,'import_textures':False})[0]
for item in json.loads((P/'unreal_materials.json').read_text()):
 assert c.call('mesh','set_material',{'mesh':mesh,'slot_name':item['slot'],'material':item['material']})
comps=c.call('editor_toolset.toolsets.actor.ActorTools','get_components',{'actor':info['model']});comp=comps[0];props=c.call('object','list_properties',{'instance':comp});assert 'staticMesh' in json.loads(props)
assert c.call('object','set_properties',{'instance':comp,'values':json.dumps({'staticMesh':mesh})})
assert c.call('asset','save_assets',{'asset_paths':[mesh['refPath'],info['level']]})
(P/'unreal_import_final.json').write_text(json.dumps({'mesh':mesh,'bounds_cm':c.call('mesh','get_bounds',{'mesh':mesh}),'slots':c.call('mesh','get_material_slots',{'mesh':mesh}),'previous_mesh':'SM_HansaBakery (superseded staging comparison)'},indent=2))
c.call('scene','load_level',{'level_path':info['level']});print('FINAL_MESH_SAVED_REOPENED',mesh)
