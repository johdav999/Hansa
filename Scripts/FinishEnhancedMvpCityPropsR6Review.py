"""Resume after native duplicate creates an unsaved sibling review map."""
import json
from RostockTerrainSession import JOB, call
G='editor_toolset.toolsets.'; AS=G+'asset.AssetTools'; SC=G+'scene.SceneTools'
ROOT='/Game/Hansa/Generated/Staging/CityLife_P20'
OLD=ROOT+'/L_CityLife_Review'; LEVEL=OLD+'_r6'
context=call('HansaEditor.HansaCityTerrainToolset','InspectAuthoringContext')
if isinstance(context,str): context=json.loads(context)
assert not context['pieRunning']
assert all(p==LEVEL for p in context['dirtyPackages']), context
assert call(SC,'get_current_level')==OLD
assert call(AS,'save_assets',asset_paths=[LEVEL])
call(SC,'load_level',level_path=LEVEL)
assert call(SC,'get_current_level')==LEVEL
placement=json.loads((JOB/'evidence/prop-review-placement.json').read_text())
manifest=json.loads((JOB/'exports-r6/export-manifest.json').read_text())
records=json.loads((JOB/'evidence/unreal-street-meshes-r6.json').read_text())
assert set(records)==set(placement['actors'])==set(manifest['modules'])
result={'level':LEVEL,'actors':{},'floorTopCm':placement['floorTopCm']}
for name,actor in placement['actors'].items():
    old_actor={'refPath':actor['refPath'].replace(OLD+'.L_CityLife_Review',LEVEL+'.L_CityLife_Review_r6')}
    assert old_actor['refPath'].startswith(LEVEL+'.')
    assert call(SC,'remove_from_scene',actor=old_actor)
    x,y,z=manifest['roleOffsetsMetres'][name]
    result['actors'][name]=call(SC,'add_to_scene_from_asset',asset_path=records[name]['mesh']['refPath'],name='P20R6_'+name,
        xform={'location':{'x':x*100,'y':-y*100,'z':result['floorTopCm']},
               'rotation':{'pitch':0,'yaw':0,'roll':0},'scale':{'x':1,'y':1,'z':1}})
assert call(AS,'save_assets',asset_paths=[LEVEL])
call(SC,'load_level',level_path=LEVEL)
result['savedAndReopened']=call(SC,'get_current_level')==LEVEL
(JOB/'evidence/prop-review-placement-r6.json').write_text(json.dumps(result,indent=2))
print('R6 draft saved and reopened; previous draft untouched.')
