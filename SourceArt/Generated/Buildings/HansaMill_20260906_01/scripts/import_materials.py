from unreal_ops import Client,P
import json
c=Client();root='/Game/Hansa/Generated/Staging/HansaMill_20260906_01';inv=json.loads((P/'material_inventory.json').read_text());classes=json.loads((P/'unreal_classes.json').read_text());seed=json.loads((P/'unreal_seed_nodes.json').read_text());results=[]
def props(o,v):
 schema=c.call('object','list_properties',{'instance':o});schema=json.loads(schema) if isinstance(schema,str) else schema;assert all(k in schema for k in v),(v,schema)
 assert c.call('object','set_properties',{'instance':o,'values':json.dumps(v)})
def node(mat,name):return c.call('material','add_expression',{'material_or_function':mat,'expression_class':next(x for x in classes if x['refPath'].endswith('.MaterialExpression'+name))})
def link(a,out,b,inp):return c.call('material','connect_expressions',{'from_expression':a,'from_output_name':out,'to_expression':b,'to_input_name':inp})
for rec in inv:
 name=rec['name'];path=root+'/Materials/M_Mill_'+name
 if name=='Oak':mat=json.loads((P/'unreal_seed.json').read_text())
 else:
  assert not c.call('asset','exists',{'path':path});mat=c.call('material','create_material',{'folder_path':root+'/Materials','asset_name':'M_Mill_'+name})
 nodes={};textures={}
 for kind,src in rec['maps'].items():
  tp=root+'/Textures/T_Mill_'+name+'_'+kind
  if c.call('asset','exists',{'path':tp}):tex=c.call('asset','load_asset',{'asset_path':tp})
  else:tex=c.call('texture','import_file',{'folder_path':root+'/Textures','asset_name':'T_Mill_'+name+'_'+kind,'source_file':src})[0]
  v={'sRGB':kind=='BaseColor','compressionSettings':{'BaseColor':'TC_Default','Roughness':'TC_Masks','Normal':'TC_Normalmap'}[kind],'addressX':'TA_Wrap','addressY':'TA_Wrap'}
  if kind=='Normal':v['bFlipGreenChannel']=True
  props(tex,v);sample=seed['TextureSample'] if name=='Oak' and kind=='BaseColor' else node(mat,'TextureSample');props(sample,{'texture':tex,'samplerType':{'BaseColor':'SAMPLERTYPE_Color','Normal':'SAMPLERTYPE_Normal','Roughness':'SAMPLERTYPE_Masks'}[kind]});nodes[kind]=sample;textures[kind]=tex
  if kind!='BaseColor':c.call('material','connect_to_output',{'expression':sample,'output_name':'RGB' if kind=='Normal' else 'R','material_property':'MP_Normal' if kind=='Normal' else 'MP_Roughness'})
 vc=seed['VertexColor'] if name=='Oak' else node(mat,'VertexColor');mul=seed['Multiply'] if name=='Oak' else node(mat,'Multiply');link(nodes['BaseColor'],'RGB',mul,'A');link(vc,'',mul,'B');c.call('material','connect_to_output',{'expression':mul,'output_name':'','material_property':'MP_BaseColor'})
 met=seed['Constant'] if name=='Oak' else node(mat,'Constant');props(met,{'r':.62 if name=='ForgedIron' else 0});c.call('material','connect_to_output',{'expression':met,'output_name':'','material_property':'MP_Metallic'})
 c.call('material','recompile',{'material_or_function':mat});assert c.call('asset','save_assets',{'asset_paths':[mat['refPath']]+[t['refPath'] for t in textures.values()]})
 results.append({'name':name,'material':mat,'textures':textures});(P/'unreal_materials.json').write_text(json.dumps(results,indent=2));print('MATERIAL_SAVED',name,flush=True)
for rec in results:
 for kind,tex in rec['textures'].items():
  size=c.call('texture','get_size',{'texture':tex});expected=1254 if kind=='BaseColor' else 1024;assert size=={'x':expected,'y':expected},size
print('MATERIALS_VERIFIED',len(results),flush=True)
