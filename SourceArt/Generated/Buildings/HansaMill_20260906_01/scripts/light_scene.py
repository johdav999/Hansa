from unreal_ops import Client,P
import json
c=Client();i=json.loads((P/'preview_actors.json').read_text());actor='editor_toolset.toolsets.actor.ActorTools'
def setp(o,v):
 p=c.call('object','list_properties',{'instance':o});p=json.loads(p) if isinstance(p,str) else p;assert all(k in p for k in v);assert c.call('object','set_properties',{'instance':o,'values':json.dumps(v)})
setp(i['sun']['component'],{'intensity':12000,'bUseTemperature':True,'temperature':6500,'lightSourceAngle':1.0,'mobility':'Movable'})
c.call(actor,'set_actor_transform',{'actor':i['sun']['actor'],'xform':{'rotation':{'pitch':-48,'yaw':-135,'roll':0}}})
setp(i['sky']['component'],{'intensity':1.0,'bRealTimeCapture':True,'mobility':'Movable'})
mat=c.call('material','create_material',{'folder_path':i['level'].rsplit('/',1)[0],'asset_name':'M_MillPreview_Ground'})
for val,prop in [(.18,'MP_BaseColor'),(.92,'MP_Roughness')]:
 n=c.call('material','add_expression',{'material_or_function':mat,'expression_class':{'refPath':'/Script/Engine.MaterialExpressionConstant'}});setp(n,{'r':val});c.call('material','connect_to_output',{'expression':n,'output_name':'','material_property':prop})
c.call('material','recompile',{'material_or_function':mat});comp=c.call(actor,'get_components',{'actor':i['ground']})[0];setp(comp,{'overrideMaterials':[mat]})
i['ground_material']=mat;i['sun_readback']=c.call(actor,'get_actor_transform',{'actor':i['sun']['actor']});(P/'preview_actors.json').write_text(json.dumps(i,indent=2));assert c.call('asset','save_assets',{'asset_paths':[i['level'],mat['refPath']]});c.call('scene','load_level',{'level_path':i['level']});print('SAVED_REOPENED',i['level'])
