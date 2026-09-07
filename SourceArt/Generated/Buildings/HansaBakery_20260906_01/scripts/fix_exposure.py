from unreal_ops import Client,P
import json
c=Client();i=json.loads((P/'preview_actors.json').read_text());pp=i['postprocess'];v={'bUnbound':True,'priority':100,'blendWeight':1,'settings':{'bOverride_AutoExposureMinBrightness':True,'bOverride_AutoExposureMaxBrightness':True,'bOverride_AutoExposureBias':True,'autoExposureMinBrightness':12,'autoExposureMaxBrightness':12,'autoExposureBias':0}}
print(c.call('object','set_properties',{'instance':pp,'values':json.dumps(v)}))
print(c.call('object','get_properties',{'instance':i['DirectionalLight_0_components'][0],'properties':['intensity','mobility','castShadows']}))
assert c.call('asset','save_assets',{'asset_paths':[i['level']]});c.call('scene','load_level',{'level_path':i['level']})
