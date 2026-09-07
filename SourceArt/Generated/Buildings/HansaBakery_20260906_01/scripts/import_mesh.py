from unreal_ops import Client,P
import json
c=Client();preview='/Game/Hansa/Developer/GenerationPreview/HansaBakery_20260906_01/L_BakeryPreview'
c.call('scene','load_level',{'level_path':preview});print(c.call('scene','get_current_level'))
print(json.dumps(c.call('scene','find_actors',{'name':'','tag':'','collision_channels':[]})))
(P/'unreal_preview.json').write_text(json.dumps({'previous_level':'/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP','preview_level':preview,'staging_root':'/Game/Hansa/Generated/Staging/HansaBakery_20260906_01'},indent=2))
root='/Game/Hansa/Generated/Staging/HansaBakery_20260906_01'
assert not c.call('asset','exists',{'path':root+'/Meshes/SM_HansaBakery'})
r=c.call('mesh','import_file',{'folder_path':root+'/Meshes','asset_name':'SM_HansaBakery','source_file':str(P/'exports'/'HansaBakery.fbx'),'import_materials':False,'import_textures':False,'combine_meshes':True})
(P/'unreal_import.json').write_text(json.dumps(r,indent=2));print('Imported',r)
