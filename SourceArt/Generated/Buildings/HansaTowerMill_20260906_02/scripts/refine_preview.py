from unreal_ops import Client,P
import json
c=Client();level=json.loads((P/'preview_actors.json').read_text())['level'];sun={'refPath':level+'.L_TowerPreview:PersistentLevel.DirectionalLight_0.LightComponent0'}
c.call('object','list_properties',{'instance':sun});props=c.call('object','get_properties',{'instance':sun,'properties':['intensity','lightSourceAngle']});print(props)
assert c.call('object','set_properties',{'instance':sun,'values':json.dumps({'lightSourceAngle':4.0})});assert c.call('asset','save_assets',{'asset_paths':[level]})
p=P/'scripts/capture_unreal.py';s=p.read_text().replace('(0,2400,850),(0,0,850)','(0,1700,870),(0,0,870)').replace('(1500,2200,1400),(0,0,850)','(1100,1700,1250),(0,0,850)').replace('(-1600,-2400,1200)','(-1200,-1900,1200)');p.write_text(s)
