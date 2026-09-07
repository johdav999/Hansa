from unreal_ops import Client,P
import json
c=Client();i=json.loads((P/'preview_actors.json').read_text());s={'bUnbound':True,'priority':100,'blendWeight':1,'settings':{'bOverride_AutoExposureMinBrightness':True,'bOverride_AutoExposureMaxBrightness':True,'autoExposureMinBrightness':12,'autoExposureMaxBrightness':12,'bOverride_AutoExposureBias':True,'autoExposureBias':0,'bOverride_MotionBlurAmount':True,'motionBlurAmount':0}}
c.call('object','list_properties',{'instance':i['postprocess']});assert c.call('object','set_properties',{'instance':i['postprocess'],'values':json.dumps(s)});assert c.call('asset','save_assets',{'asset_paths':[i['level']]});c.call('scene','load_level',{'level_path':i['level']})
# Match installed tool behavior: redraw several frames for temporal shading to settle.
p=P/'scripts/capture_unreal.py';code=p.read_text();code=code.replace("c.call('app','SetCameraTransform',{'transform':pose});out,_=rpc", "c.call('app','SetCameraTransform',{'transform':pose})\nfor warmup in range(24):\n out,_=rpc");p.write_text(code)
