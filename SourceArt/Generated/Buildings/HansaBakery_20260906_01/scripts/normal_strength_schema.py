from unreal_ops import Client,P
import json
c=Client();d=json.loads((P/'scripts/schema_material.decoded.json').read_text())
for t in d['tools']:
 if t['name'].split('.')[-1] in ['get_property_input','connect_expressions']:print(json.dumps(t))
materials=json.loads((P/'unreal_materials.json').read_text());m=next(x['material'] for x in materials if x['name']=='Brick_0');n=c.call('material','add_expression',{'material_or_function':m,'expression_class':{'refPath':'/Script/Engine.MaterialExpressionLinearInterpolate'},'x':-180,'y':450});props=c.call('object','list_properties',{'instance':n});print(props);(P/'normal_strength_seed.json').write_text(json.dumps({'node':n,'material':m}));(P/'scripts/props_lerp.json').write_text(props)
