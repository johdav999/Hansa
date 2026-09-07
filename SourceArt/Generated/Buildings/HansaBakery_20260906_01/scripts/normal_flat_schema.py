from unreal_ops import Client,P
import json
c=Client();seed=json.loads((P/'normal_strength_seed.json').read_text());n=c.call('material','add_expression',{'material_or_function':seed['material'],'expression_class':{'refPath':'/Script/Engine.MaterialExpressionConstant3Vector'},'x':-380,'y':700});print(c.call('object','list_properties',{'instance':n}));print(c.call('material','get_expression_input_names',{'expression':seed['node']}));seed['flat']=n;(P/'normal_strength_seed.json').write_text(json.dumps(seed))
