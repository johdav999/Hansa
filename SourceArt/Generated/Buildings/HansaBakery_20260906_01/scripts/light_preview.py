from unreal_ops import Client,P
import json,math
c=Client();info=json.loads((P/'preview_actors.json').read_text());actor_ts='editor_toolset.toolsets.actor.ActorTools'
def setp(o,v):
 c.call('object','list_properties',{'instance':o});assert c.call('object','set_properties',{'instance':o,'values':json.dumps(v)})
mat=c.call('material','create_material',{'folder_path':'/Game/Hansa/Developer/GenerationPreview/HansaBakery_20260906_01','asset_name':'M_Preview_Ground'})
for value,prop,y in [(.22,'MP_BaseColor',0),(.9,'MP_Roughness',200)]:
 node=c.call('material','add_expression',{'material_or_function':mat,'expression_class':{'refPath':'/Script/Engine.MaterialExpressionConstant'},'x':-300,'y':y});setp(node,{'r':value});c.call('material','connect_to_output',{'expression':node,'output_name':'','material_property':prop})
c.call('material','recompile',{'material_or_function':mat});setp(info['ground_components'][0],{'overrideMaterials':[mat]})
sun=info['DirectionalLight_0_components'][0];setp(sun,{'intensity':60000,'bUseTemperature':True,'temperature':6500,'lightSourceAngle':1.0,'mobility':'Movable'})
sun_actor={'refPath':sun['refPath'].rsplit('.',1)[0]};c.call(actor_ts,'set_actor_transform',{'actor':sun_actor,'xform':{'rotation':{'pitch':-48,'yaw':-135,'roll':0}}})
setp(info['SkyLight_0_components'][1],{'intensity':1.0,'bRealTimeCapture':True,'mobility':'Movable'})
pose={'location':{'x':2400,'y':3300,'z':2100},'rotation':{'pitch':-18.36,'yaw':-126.03,'roll':0}}
c.call('app','SetCameraTransform',{'transform':pose})
assert c.call('asset','save_assets',{'asset_paths':[info['level'],mat['refPath']]})
info['hero_pose']=pose;info['ground_material']=mat;(P/'preview_actors.json').write_text(json.dumps(info,indent=2))
print('Saved',info['level']);print('sun',c.call(actor_ts,'get_actor_transform',{'actor':sun_actor}))
c.call('scene','load_level',{'level_path':info['level']})
print('Reopened',c.call('scene','get_current_level'))
