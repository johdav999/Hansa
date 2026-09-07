from unreal_ops import Client,P
import json
c=Client();m=json.loads((P/'unreal_seed.json').read_text());classes=json.loads((P/'unreal_classes.json').read_text());found={}
for name in ['TextureSample','Multiply','VertexColor','Constant']:
 cl=next(x for x in classes if x['refPath'].endswith('.MaterialExpression'+name));e=c.call('material','add_expression',{'material_or_function':m,'expression_class':cl});props=c.call('object','list_properties',{'instance':e});found[name]=e;(P/'scripts'/('props_'+name+'.json')).write_text(props);print(name,props[:2200])
(P/'unreal_seed_nodes.json').write_text(json.dumps(found,indent=2))
