from unreal_ops import Client,P
import json
c=Client();root='/Game/Hansa/Generated/Staging/HansaBakery_20260906_01';mesh=json.loads((P/'unreal_import.json').read_text())[0];seed=json.loads((P/'unreal_material_seed.json').read_text());inv=json.loads((P/'material_inventory.json').read_text());result=[]
def ensured(path,create):
 if c.call('asset','exists',{'path':path}):return c.call('asset','load_asset',{'asset_path':path})
 return create()
def setprops(obj,values):
 props=c.call('object','list_properties',{'instance':obj});props=json.loads(props) if isinstance(props,str) else props
 assert all(k in props for k in values),set(values)-set(props)
 assert c.call('object','set_properties',{'instance':obj,'values':json.dumps(values)})
for entry in inv:
 name=entry['name'];print('MATERIAL',name,flush=True)
 mat=ensured(root+'/Materials/M_Bakery_'+name,lambda:c.call('material','create_material',{'folder_path':root+'/Materials','asset_name':'M_Bakery_'+name}))
 textures={}
 for j,(kind,src) in enumerate(entry['maps'].items()):
  tex=ensured(root+'/Textures/T_Bakery_'+name+'_'+kind,lambda:c.call('texture','import_file',{'folder_path':root+'/Textures','asset_name':'T_Bakery_'+name+'_'+kind,'source_file':src})[0])
  values={'sRGB':kind=='BaseColor','compressionSettings':{'BaseColor':'TC_Default','Normal':'TC_Normalmap','Roughness':'TC_Masks'}[kind],'addressX':'TA_Wrap','addressY':'TA_Wrap'}
  if kind=='Normal':values['bFlipGreenChannel']=True
  setprops(tex,values)
  size={'x':1024,'y':1024} # Final readback after async compilation below
  node=seed['sample'] if name=='Brick_0' and kind=='BaseColor' else c.call('material','add_expression',{'material_or_function':mat,'expression_class':{'refPath':'/Script/Engine.MaterialExpressionTextureSample'},'x':-500,'y':j*210})
  setprops(node,{'texture':tex,'samplerType':{'BaseColor':'SAMPLERTYPE_Color','Normal':'SAMPLERTYPE_Normal','Roughness':'SAMPLERTYPE_Masks'}[kind]})
  c.call('material','connect_to_output',{'expression':node,'output_name':'RGB' if kind!='Roughness' else 'R','material_property':{'BaseColor':'MP_BaseColor','Normal':'MP_Normal','Roughness':'MP_Roughness'}[kind]})
  textures[kind]={'asset':tex,'size':size}
 const=seed['constant'] if name=='Brick_0' else c.call('material','add_expression',{'material_or_function':mat,'expression_class':{'refPath':'/Script/Engine.MaterialExpressionConstant'},'x':-200,'y':600})
 setprops(const,{'r':entry['metallic']})
 c.call('material','connect_to_output',{'expression':const,'output_name':'','material_property':'MP_Metallic'})
 c.call('material','recompile',{'material_or_function':mat})
 assert c.call('mesh','set_material',{'mesh':mesh,'slot_name':'PBR_'+name,'material':mat})
 paths=[mat['refPath']]+[v['asset']['refPath'] for v in textures.values()]
 assert c.call('asset','save_assets',{'asset_paths':paths})
 result.append({'name':name,'material':mat,'textures':textures,'slot':'PBR_'+name});(P/'unreal_materials.json').write_text(json.dumps(result,indent=2))
assert c.call('asset','save_assets',{'asset_paths':[mesh['refPath']]})
for item in result:
 for record in item['textures'].values():
  actual=c.call('texture','get_size',{'texture':record['asset']});assert actual==record['size'],actual
print('MATERIAL_ASSIGNMENT_COMPLETE',len(result))


