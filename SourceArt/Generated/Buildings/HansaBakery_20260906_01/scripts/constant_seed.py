from unreal_ops import Client,P
import json
c=Client();seed=json.loads((P/'unreal_material_seed.json').read_text())
const=c.call('material','add_expression',{'material_or_function':seed['material'],'expression_class':{'refPath':'/Script/Engine.MaterialExpressionConstant'},'x':-400,'y':450})
props=c.call('object','list_properties',{'instance':const});(P/'scripts'/'properties_constant.json').write_text(props);print(props)
seed['constant']=const;(P/'unreal_material_seed.json').write_text(json.dumps(seed,indent=2))
