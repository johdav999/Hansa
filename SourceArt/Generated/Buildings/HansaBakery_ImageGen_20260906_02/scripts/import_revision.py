
from unreal_ops import Client,P
import json,pathlib
c=Client();old=P.parent/'hansa-bakery_20260906_01'
identity={'project':str(P.parents[2]/'Hansa.uproject'),'job':P.name};(P/'identity.json').write_text(json.dumps(identity))
assert json.loads(c.call('asset','read_file',{'file_path':str(P/'identity.json')}))==identity
info=json.loads((old/'preview_actors.json').read_text());assert c.call('scene','get_current_level')==info['level'];assert not c.call('asset','is_dirty',{'asset_path':info['level']})
newlevel=info['level'].replace('HansaBakery_20260906_01','HansaBakery_ImageGen_20260906_02')
assert not c.call('asset','exists',{'path':newlevel})
assert c.call('asset','duplicate',{'path':info['level'],'new_path':newlevel})
assert c.call('asset','save_assets',{'asset_paths':[newlevel]});c.call('scene','load_level',{'level_path':newlevel})
info=json.loads(json.dumps(info).replace('HansaBakery_20260906_01','HansaBakery_ImageGen_20260906_02'));(P/'preview_actors.json').write_text(json.dumps(info,indent=2))
root='/Game/Hansa/Generated/Staging/HansaBakery_ImageGen_20260906_02'
records=json.loads((old/'unreal_materials.json').read_text());inv={e['name']:e for e in json.loads((P/'material_inventory.json').read_text())}
for r in records:
 e=inv[r['name']]
 if 'imagegen_source' not in e:continue
 name=r['name'];dest=root+'/Materials/M_Bakery_'+name
 assert not c.call('asset','exists',{'path':dest})
 assert c.call('asset','duplicate',{'path':r['material']['refPath'].split('.')[0],'new_path':dest})
 mat=c.call('asset','load_asset',{'asset_path':dest})
 tex=c.call('texture','import_file',{'folder_path':root+'/Textures','asset_name':'T_Bakery_'+name+'_ImageGen_BaseColor','source_file':e['maps']['BaseColor']})[0]
 c.call('object','list_properties',{'instance':tex})
 assert c.call('object','set_properties',{'instance':tex,'values':json.dumps({'sRGB':True,'compressionSettings':'TC_Default','addressX':'TA_Wrap','addressY':'TA_Wrap'})})
 src=c.call('material','get_property_input',{'material':mat,'material_property':'MP_BaseColor'})
 c.call('object','list_properties',{'instance':src['expression']})
 assert c.call('object','set_properties',{'instance':src['expression'],'values':json.dumps({'texture':tex,'samplerType':'SAMPLERTYPE_Color'})})
 c.call('material','recompile',{'material_or_function':mat});assert c.call('asset','save_assets',{'asset_paths':[mat['refPath'],tex['refPath']]})
 r['material']=mat;r['textures']['BaseColor']={'asset':tex,'size':{'x':1254,'y':1254}}
 (P/'unreal_materials.json').write_text(json.dumps(records,indent=2))
 print('UPDATED',name,flush=True)
mesh=c.call('mesh','import_file',{'folder_path':root+'/Meshes','asset_name':'SM_HansaBakery_ImageGen','source_file':str(P/'exports/HansaBakery.fbx'),'combine_meshes':True,'import_materials':False,'import_textures':False})[0]
for r in records:assert c.call('mesh','set_material',{'mesh':mesh,'slot_name':r['slot'],'material':r['material']})
comp=c.call('editor_toolset.toolsets.actor.ActorTools','get_components',{'actor':info['model']})[0]
c.call('object','list_properties',{'instance':comp});assert c.call('object','set_properties',{'instance':comp,'values':json.dumps({'staticMesh':mesh})})
assert c.call('asset','save_assets',{'asset_paths':[mesh['refPath'],newlevel]});c.call('scene','load_level',{'level_path':newlevel})
checks=[]
for r in records:
 assert c.call('mesh','get_material',{'mesh':mesh,'slot_name':r['slot']})==r['material']
 for k,t in r['textures'].items():
  actual=c.call('texture','get_size',{'texture':t['asset']});assert actual==t['size'],(r['name'],k,actual)
 checks.append(r['slot'])
(P/'unreal_import_final.json').write_text(json.dumps({'mesh':mesh,'bounds_cm':c.call('mesh','get_bounds',{'mesh':mesh}),'slots_verified':checks,'level_saved_reopened':newlevel,'revised_material_count':9,'normal_roughness':'retained original physical maps and prior engine normal adaptation','approval':'staging'},indent=2))
print('UNREAL_REVISION_VERIFIED',flush=True)

