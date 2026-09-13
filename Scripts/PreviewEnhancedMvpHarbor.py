"""Isolated saved/reopened P17 review level; never edits either gameplay city."""
import sys,json,math,time,base64
from pathlib import Path
REPO=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(REPO/'SourceArt/Generated/Buildings/HansaSawmill_P13_20260908/scripts'))
from ue_batch import call
JOB=REPO/'Saved/GenerationJobs/hansa-harbor_P17_20260908'
AS='editor_toolset.toolsets.asset.AssetTools';SC='editor_toolset.toolsets.scene.SceneTools'
ROOT='/Game/Hansa/Generated/Staging/Harbor_P17';LEVEL=ROOT+'/L_Harbor_Review'
if sys.argv[1]=='create':
    current=call(SC,'get_current_level');assert not call(AS,'is_dirty',asset_path=current)
    assert not call(AS,'exists',path=LEVEL)
    assert call(AS,'duplicate',path='/Game/Hansa/Generated/Staging/Market_P15/L_Market_Review',new_path=LEVEL)
    call(AS,'save_assets',asset_paths=[LEVEL]);call(SC,'load_level',level_path=LEVEL)
    for actor in call(SC,'find_actors',name='',tag='',collision_channels=[]):
        if 'BP_Market_' in actor['refPath'] or 'SM_HansaMarket' in actor['refPath']:
            assert actor['refPath'].startswith(LEVEL+'.');assert call(SC,'remove_from_scene',actor=actor)
    actor=call(SC,'add_to_scene_from_asset',asset_path=ROOT+'/BP_Harbor_Review',name='P17_Harbor_Review',xform={'location':{'x':0,'y':0,'z':225},'rotation':{'pitch':0,'yaw':0,'roll':0},'scale':{'x':1,'y':1,'z':1}})
    call(AS,'save_assets',asset_paths=[LEVEL]);call(SC,'load_level',level_path=LEVEL)
    (JOB/'evidence/preview-reopen.json').write_text(json.dumps({'level':call(SC,'get_current_level'),'actor':actor},indent=2))
    print('Saved and reopened',LEVEL)
else:
    assert call(SC,'get_current_level')==LEVEL
    for tag,distance,target,pitch,yaw in [('25m',2500,(0,0,225),-40,145),('65m',6500,(0,0,225),-55,145),('120m',12000,(0,0,225),-55,145),('detail',1000,(600,-70,370),-25,145)]:
        horizontal=distance*math.cos(math.radians(-pitch))
        transform={'location':{'x':target[0]-horizontal*math.cos(math.radians(yaw)),'y':target[1]-horizontal*math.sin(math.radians(yaw)),'z':target[2]+distance*math.sin(math.radians(-pitch))},'rotation':{'pitch':pitch,'yaw':yaw,'roll':0},'scale':{'x':1,'y':1,'z':1}}
        call('EditorToolset.EditorAppToolset','SetCameraTransform',transform=transform);time.sleep(2)
        data=call('EditorToolset.EditorAppToolset','CaptureViewport',captureTransform=transform,bShowUI=False,annotations={'gridSpacing':0,'gridExtent':0,'gridHeight':0,'maxLabelDistance':0,'classFilter':{'refPath':'/Script/Engine.Actor'},'maxLabels':0})
        (JOB/'renders'/('unreal-'+tag+'.png')).write_bytes(base64.b64decode(data.pop('image')['data']))
        (JOB/'evidence'/('unreal-'+tag+'.json')).write_text(json.dumps(data,indent=2));print('Captured',tag,flush=True)
