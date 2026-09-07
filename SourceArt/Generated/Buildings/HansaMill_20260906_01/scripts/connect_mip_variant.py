from unreal_ops import Client,P
import json
c=Client();inv={r['name']:r for r in json.loads((P/'material_inventory.json').read_text())};records=json.loads((P/'unreal_materials.json').read_text());root='/Game/Hansa/Generated/Staging/HansaMill_20260906_01'
def setp(o,v):
 schema=c.call('object','list_properties',{'instance':o});schema=json.loads(schema) if isinstance(schema,str) else schema;assert all(k in schema for k in v);assert c.call('object','set_properties',{'instance':o,'values':json.dumps(v)})
for rec in records:
 name=rec['name'];assert not c.call('asset','exists',{'path':root+'/Textures/T_Mill_'+name+'_BaseColor_Native1024'})
 tex=c.call('texture','import_file',{'folder_path':root+'/Textures','asset_name':'T_Mill_'+name+'_BaseColor_Native1024','source_file':inv[name]['maps']['BaseColor']})[0];setp(tex,{'sRGB':True,'compressionSettings':'TC_Default','addressX':'TA_Wrap','addressY':'TA_Wrap'})
 src=c.call('material','get_property_input',{'material':rec['material'],'material_property':'MP_BaseColor'});connections=c.call('material','get_expression_inputs',{'material_or_function':rec['material'],'expression':src['expression']})
 sample=next(x['expression'] for x in connections if x['input_name']=='A');setp(sample,{'texture':tex,'samplerType':'SAMPLERTYPE_Color'});rec['original_color_texture']=rec['textures']['BaseColor'];rec['textures']['BaseColor']=tex;c.call('material','recompile',{'material_or_function':rec['material']});assert c.call('asset','save_assets',{'asset_paths':[tex['refPath'],rec['material']['refPath']]});assert c.call('texture','get_size',{'texture':tex})=={'x':1024,'y':1024}
 print('NATIVE1024_CONNECTED',name,flush=True)
(P/'unreal_materials.json').write_text(json.dumps(records,indent=2));i=json.loads((P/'preview_actors.json').read_text());setp(i['postprocess'],{'settings':{'bOverride_AutoExposureMinBrightness':True,'bOverride_AutoExposureMaxBrightness':True,'autoExposureMinBrightness':11.3,'autoExposureMaxBrightness':11.3,'bOverride_AutoExposureBias':True,'autoExposureBias':0}});assert c.call('asset','save_assets',{'asset_paths':[i['level']]});c.call('scene','load_level',{'level_path':i['level']})
