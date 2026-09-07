from unreal_ops import Client,P
import json
c=Client();r=next(r for r in json.loads((P/'unreal_materials.json').read_text()) if r['name']=='Weathered_Oak');m=r['material'];src=c.call('material','get_property_input',{'material':m,'material_property':'MP_Normal'})
lerp=c.call('material','add_expression',{'material_or_function':m,'expression_class':{'refPath':'/Script/Engine.MaterialExpressionLinearInterpolate'},'x':-180,'y':450})
flat=c.call('material','add_expression',{'material_or_function':m,'expression_class':{'refPath':'/Script/Engine.MaterialExpressionConstant3Vector'},'x':-380,'y':700})
for n,v in [(flat,{'constant':{'r':0,'g':0,'b':1,'a':1}}),(lerp,{'constAlpha':.9})]:
 c.call('object','list_properties',{'instance':n});assert c.call('object','set_properties',{'instance':n,'values':json.dumps(v)})
c.call('material','connect_expressions',{'from_expression':src['expression'],'from_output_name':src['output_name'],'to_expression':lerp,'to_input_name':'A'})
c.call('material','connect_expressions',{'from_expression':flat,'from_output_name':'','to_expression':lerp,'to_input_name':'B'})
c.call('material','connect_to_output',{'expression':lerp,'output_name':'','material_property':'MP_Normal'})
c.call('material','recompile',{'material_or_function':m});assert c.call('asset','save_assets',{'asset_paths':[m['refPath']]})
(P/'evidence/unreal_oak_normal.json').write_text(json.dumps({'material':m,'sample_weight':.1,'flat_weight':.9,'reason':'Reduce overly strong retained grain relief under engine daylight; color input unchanged'},indent=2))
print('OAK_NORMAL_ADAPTED')
