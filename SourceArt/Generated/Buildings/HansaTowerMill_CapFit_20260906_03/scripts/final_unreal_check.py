from unreal_ops import Client,P
import json
c=Client();i=json.loads((P/'preview_actors.json').read_text());mesh=json.loads((P/'unreal_mesh.json').read_text());records=json.loads((P/'unreal_materials.json').read_text());result={'project':str(P.parents[2]/'Hansa.uproject'),'mesh':mesh,'preview':i['level'],'status':'verified staging import; production promotion pending user review','materials':[]}
schema=c.call('object','list_properties',{'instance':mesh});schema=json.loads(schema) if isinstance(schema,str) else schema
if 'assetImportData' in schema:
 data=c.call('object','get_properties',{'instance':mesh,'properties':['assetImportData']});(P/'unreal_import_properties.json').write_text(json.dumps(data,indent=2))
for r in records:
 assert c.call('mesh','get_material',{'mesh':mesh,'slot_name':r['name']})==r['material'];c.call('material','recompile',{'material_or_function':r['material']})
 texs={}
 for kind,t in r['textures'].items():
  size=c.call('texture','get_size',{'texture':t});assert size=={'x':1024,'y':1024};c.call('object','list_properties',{'instance':t});props=c.call('object','get_properties',{'instance':t,'properties':['sRGB','compressionSettings','mipGenSettings']});texs[kind]={'texture':t,'dimensions':size,'settings':props}
 result['materials'].append({'slot':r['name'],'material':r['material'],'textures':texs})
assert c.call('asset','save_assets',{'asset_paths':[mesh['refPath'],i['level']]+[r['material']['refPath'] for r in records]});c.call('scene','load_level',{'level_path':i['level']});assert c.call('scene','get_current_level')==i['level'];assert not c.call('asset','is_dirty',{'asset_path':i['level']})
result['saved_reopened']=True;result['bounds_cm']=c.call('mesh','get_bounds',{'mesh':mesh});result['triangles']=c.call('mesh','get_triangle_count',{'mesh':mesh});result['lod_count']=c.call('mesh','get_lod_count',{'mesh':mesh});source=json.loads((P/'exports/geometry.json').read_text())['bounds_m'];bounds=result['bounds_cm'];expected={'min':{'x':source[0][0]*100,'y':-source[1][1]*100,'z':source[0][2]*100},'max':{'x':source[1][0]*100,'y':-source[0][1]*100,'z':source[1][2]*100}}
assert max(abs(bounds[a][k]-expected[a][k]) for a in ['min','max'] for k in 'xyz')<.1;result['unit_and_handedness_verified']=True;(P/'unreal_final_verification.json').write_text(json.dumps(result,indent=2));print('FINAL_UNREAL_CHECK_PASSED')
