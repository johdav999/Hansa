from unreal_ops import Client,P
import json
c=Client();root='/Game/Hansa/Generated/Staging/HansaMill_20260906_01';name='SM_HansaMill';assert not c.call('asset','exists',{'path':root+'/Meshes/'+name})
mesh=c.call('mesh','import_file',{'folder_path':root+'/Meshes','asset_name':name,'source_file':str(P/'exports/HansaMill.fbx'),'import_materials':False,'import_textures':False,'combine_meshes':True})[0]
(P/'unreal_mesh.json').write_text(json.dumps(mesh))
slots=c.call('mesh','get_material_slots',{'mesh':mesh});print('SLOTS',slots)
for r in json.loads((P/'unreal_materials.json').read_text()):
 assert r['name'] in str(slots),r['name'];assert c.call('mesh','set_material',{'mesh':mesh,'slot_name':r['name'],'material':r['material']})
 assert c.call('mesh','get_material',{'mesh':mesh,'slot_name':r['name']})==r['material']
assert c.call('asset','save_assets',{'asset_paths':[mesh['refPath']]});bounds=c.call('mesh','get_bounds',{'mesh':mesh});count=c.call('mesh','get_triangle_count',{'mesh':mesh})
(P/'unreal_geometry.json').write_text(json.dumps({'mesh':mesh,'bounds_cm':bounds,'triangles':count,'slots':slots},indent=2));print('MESH_VERIFIED',bounds,count)
