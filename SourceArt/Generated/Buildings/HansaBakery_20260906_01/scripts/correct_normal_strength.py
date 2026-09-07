from unreal_ops import Client,P
import json
c=Client();seed=json.loads((P/'normal_strength_seed.json').read_text());records=[]
for item in json.loads((P/'unreal_materials.json').read_text()):
 if not item['name'].startswith(('Brick_','Clay_','Lime_')) and item['name']!='Limestone':continue
 m=item['material'];src=c.call('material','get_property_input',{'material':m,'material_property':'MP_Normal'})
 lerp=seed['node'] if item['name']=='Brick_0' else c.call('material','add_expression',{'material_or_function':m,'expression_class':{'refPath':'/Script/Engine.MaterialExpressionLinearInterpolate'},'x':-180,'y':450})
 flat=seed['flat'] if item['name']=='Brick_0' else c.call('material','add_expression',{'material_or_function':m,'expression_class':{'refPath':'/Script/Engine.MaterialExpressionConstant3Vector'},'x':-380,'y':700})
 for n,v in [(flat,{'constant':{'r':0,'g':0,'b':1,'a':1}}),(lerp,{'constAlpha':.75})]:
  c.call('object','list_properties',{'instance':n});assert c.call('object','set_properties',{'instance':n,'values':json.dumps(v)})
 c.call('material','connect_expressions',{'from_expression':src['expression'],'from_output_name':src['output_name'],'to_expression':lerp,'to_input_name':'A'})
 c.call('material','connect_expressions',{'from_expression':flat,'from_output_name':'','to_expression':lerp,'to_input_name':'B'})
 c.call('material','connect_to_output',{'expression':lerp,'output_name':'','material_property':'MP_Normal'})
 c.call('material','recompile',{'material_or_function':m});assert c.call('asset','save_assets',{'asset_paths':[m['refPath']]});records.append({'material':m,'map_normal_weight':.25,'flat_normal_weight':.75})
(P/'unreal_normal_strength.json').write_text(json.dumps(records,indent=2));print('NORMALS_ADAPTED',len(records))
i=json.loads((P/'preview_actors.json').read_text());c.call('scene','load_level',{'level_path':i['level']})
