from unreal_ops import Client,P
import json
c=Client();current=c.call('scene','get_current_level');assert current=='/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP';assert not c.call('asset','is_dirty',{'asset_path':current})
preview='/Game/Hansa/Developer/GenerationPreview/HansaBakery_20260906_01/L_BakeryPreview'
assert not c.call('asset','exists',{'path':preview})
print('duplicate',c.call('asset','duplicate',{'path':'/Engine/Maps/Templates/Template_Default','new_path':preview}))
c.call('scene','load_level',{'level_path':preview})
print('level',c.call('scene','get_current_level'))
print('actors',c.call('scene','find_actors',{'name':'','tag':'','collision_channels':[]}))
(P/'unreal_preview.json').write_text(json.dumps({'previous_level':current,'preview_level':preview,'staging_root':'/Game/Hansa/Generated/Staging/HansaBakery_20260906_01'},indent=2))
