"""Create an isolated saved staging preview through verified MCP toolsets."""
from pathlib import Path
import sys,json,base64
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'SourceArt/Generated/Trees/LubeckSummer/v1/scripts'))
from ue_batch import call
A='editor_toolset.toolsets.asset.AssetTools';S='editor_toolset.toolsets.scene.SceneTools'
O='editor_toolset.toolsets.object.ObjectTools';ACT='editor_toolset.toolsets.actor.ActorTools'
LEVEL='/Game/Hansa/Generated/Staging/FirewoodModel/L_FirewoodPreview'
JOB=ROOT/'Saved/GenerationJobs/Firewood_20260916'
def ref(p):return {'refPath':p}
def props(o,**p):return call(O,'set_properties',instance=o,values=json.dumps(p))
ctx=json.loads(call('HansaEditor.HansaCityTerrainToolset','InspectAuthoringContext'))
assert Path(ctx['projectFile']).resolve()==(ROOT/'Hansa.uproject').resolve() and all(x.startswith('/Game/Hansa/Generated/Staging/FirewoodModel') for x in ctx['dirtyPackages'])
if not call(A,'exists',path=LEVEL):assert call(A,'duplicate',path='/Engine/Maps/Entry',new_path=LEVEL)
assert call(A,'save_assets',asset_paths=[])
call(S,'load_level',level_path=LEVEL)
existing=call(S,'find_actors',name='',tag='',collision_channels=[])
assert not any('FirewoodYard_Review' in a['refPath'] for a in existing),'Already populated; inspect before retry.'
yard=call(S,'add_to_scene_from_asset',asset_path='/Game/Hansa/Generated/Staging/FirewoodModel/Meshes/SM_WoodcutterYard',name='FirewoodYard_Review',xform={})
floor=call(S,'add_to_scene_from_asset',asset_path='/Engine/BasicShapes/Cube',name='PreviewOnly_Ground',xform={'location':{'x':0,'y':0,'z':-10},'scale':{'x':30,'y':30,'z':.2}})
sun=call(S,'add_to_scene_from_class',actor_type=ref('/Script/Engine.DirectionalLight'),name='ReviewSun',xform={'rotation':{'pitch':-48,'yaw':-55,'roll':0}})
suncomponent=call(ACT,'get_components',actor=sun,component_type=ref('/Script/Engine.DirectionalLightComponent'))[0]
props(suncomponent,Intensity=5.0,bAtmosphereSunLight=True)
sky=call(S,'add_to_scene_from_class',actor_type=ref('/Script/Engine.SkyLight'),name='ReviewSky',xform={})
skycomponent=call(ACT,'get_components',actor=sky,component_type=ref('/Script/Engine.SkyLightComponent'))[0]
props(skycomponent,bRealTimeCapture=True,Intensity=1.0)
call(S,'add_to_scene_from_class',actor_type=ref('/Script/Engine.SkyAtmosphere'),name='ReviewAtmosphere',xform={})
assert call(A,'save_assets',asset_paths=[])
call(S,'load_level',level_path='/Engine/Maps/Entry')
call(S,'load_level',level_path=LEVEL)
pose={'location':{'x':1300,'y':-1500,'z':1150},'rotation':{'pitch':-27,'yaw':131,'roll':0}}
call('EditorToolset.EditorAppToolset','SetCameraTransform',transform=pose)
(JOB/'preview-context.json').write_text(json.dumps({'level':LEVEL,'yard':yard,'sun':call(ACT,'get_actor_transform',actor=sun),'camera':pose},indent=2))
print('PREVIEW_REOPENED')
