from unreal_ops import Client,P
import json,shutil,hashlib,struct,zlib
c=Client();info=json.loads((P/'preview_actors.json').read_text());sun=info['DirectionalLight_0_components'][0]
assert c.call('object','set_properties',{'instance':sun,'values':json.dumps({'lightSourceAngle':6.9})})
assert c.call('asset','save_assets',{'asset_paths':[info['level']]});c.call('scene','load_level',{'level_path':info['level']})
final=json.loads((P/'unreal_import_final.json').read_text());checks=[]
for item in json.loads((P/'unreal_materials.json').read_text()):
 actual=c.call('mesh','get_material',{'mesh':final['mesh'],'slot_name':item['slot']});assert actual==item['material'],(actual,item)
 for tex in item['textures'].values():assert c.call('texture','get_size',{'texture':tex['asset']})=={'x':1024,'y':1024}
 checks.append({'slot':item['slot'],'material':actual,'native_texture_sizes':'1024x1024'})
(P/'final_unreal_verification.json').write_text(json.dumps({'mesh':final['mesh'],'bounds_cm':c.call('mesh','get_bounds',{'mesh':final['mesh']}),'slot_checks':checks,'saved_reopened_level':info['level'],'sun_lux':12000,'sun_angle_degrees':6.9,'EV100':11.5,'production_approval':'pending'},indent=2))
print('FINAL_SLOT_AND_SIZE_READBACK_PASS',len(checks))
