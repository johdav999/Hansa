from unreal_ops import Client,P
import json
c=Client();root='/Game/Hansa/Generated/Staging/HansaBakery_20260906_01';mesh=json.loads((P/'unreal_import.json').read_text())[0]
print('bounds',c.call('mesh','get_bounds',{'mesh':mesh}));print('slots',c.call('mesh','get_material_slots',{'mesh':mesh}))
t=c.call('texture','import_file',{'folder_path':root+'/Textures','asset_name':'T_Bakery_Brick_0_BaseColor','source_file':str(P/'textures'/'Brick_0_BaseColor.png')})[0]
m=c.call('material','create_material',{'folder_path':root+'/Materials','asset_name':'M_Bakery_Brick_0'})
e=c.call('material','add_expression',{'material_or_function':m,'expression_class':{'refPath':'/Script/Engine.MaterialExpressionTextureSample'}})
for name,o in [('texture',t),('sample',e),('material',m)]:
 props=c.call('object','list_properties',{'instance':o});(P/'scripts'/('properties_'+name+'.json')).write_text(props if isinstance(props,str) else json.dumps(props));print(name,str(props)[:1000])
(P/'unreal_material_seed.json').write_text(json.dumps({'texture':t,'material':m,'sample':e},indent=2))
