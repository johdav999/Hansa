"""Import corrected P20 geometry into a sibling draft and retain the r5 comparison."""
import json
from RostockTerrainSession import JOB, call

G = 'editor_toolset.toolsets.'
AS, SM, SC = G+'asset.AssetTools', G+'static_mesh.StaticMeshTools', G+'scene.SceneTools'
ROOT = '/Game/Hansa/Generated/Staging/CityLife_P20'
OLD = ROOT+'/L_CityLife_Review'
LEVEL = ROOT+'/L_CityLife_Review_r6'
context = call('HansaEditor.HansaCityTerrainToolset', 'InspectAuthoringContext')
if isinstance(context, str): context = json.loads(context)
assert not context['pieRunning'] and not context['dirtyPackages'], context
assert call(SC, 'get_current_level') == OLD
assert not call(AS, 'exists', path=LEVEL)
manifest = json.loads((JOB/'exports-r6/export-manifest.json').read_text())
checks = json.loads((JOB/'evidence/reimport-r6.json').read_text())
assert manifest['revision'] == 6 and set(checks) == {'fbx','glb'}
assert len(manifest['modules']) == 7
for name in manifest['modules']:
    assert name in checks['fbx'] and name in checks['glb']
    assert not call(AS,'exists',path=ROOT+'/Meshes_r6/'+name)
records = {}
for name, info in manifest['modules'].items():
    mesh = call(SM,'import_file',folder_path=ROOT+'/Meshes_r6',asset_name=name,
                source_file=str(JOB/'exports-r6'/info['fbx']),import_materials=False,
                import_textures=False,combine_meshes=True)[0]
    bounds = call(SM,'get_bounds',mesh=mesh)
    for i, axis in enumerate(('x','y','z')):
        coords = [p[i]*100*(-1 if axis=='y' else 1) for p in info['boundsMetres']]
        assert abs(bounds['min'][axis]-min(coords))<.2 and abs(bounds['max'][axis]-max(coords))<.2
    if 'ShoreDebris' in name: assert abs(bounds['min']['z'])<.01
    assert call(SM,'generate_lods',mesh=mesh,triangle_percents=[.5,.25]) == 3
    assert call(SM,'set_lod_thresholds',mesh=mesh,thresholds=[1,.08,.025])
    assert call(SM,'generate_convex_collisions',mesh=mesh,hull_count=4,max_hull_verts=16,hull_precision=50000)
    for slot in call(SM,'get_material_slots',mesh=mesh):
        family = slot.removeprefix('M_CityLife_').removesuffix('_Portable')
        assert family in ('Oak','Iron','Wicker')
        material = {'refPath':'/Game/Mesh/hansa-market/Materials/M_Market_'+family+'.M_Market_'+family}
        assert call(SM,'set_material',mesh=mesh,slot_name=slot,material=material)
    assert call(AS,'save_assets',asset_paths=[mesh['refPath']])
    records[name] = {'mesh':mesh,'boundsCm':bounds,'status':'staged-awaiting-native-r6-review'}
    (JOB/'evidence/unreal-street-meshes-r6.json').write_text(json.dumps(records,indent=2))
assert call(AS,'duplicate',path=OLD,new_path=LEVEL)
assert call(AS,'save_assets',asset_paths=[LEVEL])
call(SC,'load_level',level_path=LEVEL)
assert call(SC,'get_current_level') == LEVEL
placement = json.loads((JOB/'evidence/prop-review-placement.json').read_text())
result = {'level':LEVEL,'actors':{},'floorTopCm':placement['floorTopCm']}
for name, actor in placement['actors'].items():
    old_actor = {'refPath':actor['refPath'].replace(OLD+'.L_CityLife_Review',LEVEL+'.L_CityLife_Review_r6')}
    assert old_actor['refPath'].startswith(LEVEL+'.')
    assert call(SC,'remove_from_scene',actor=old_actor)
    x,y,z = manifest['roleOffsetsMetres'][name]
    result['actors'][name] = call(SC,'add_to_scene_from_asset',asset_path=records[name]['mesh']['refPath'],name='P20R6_'+name,
        xform={'location':{'x':x*100,'y':-y*100,'z':result['floorTopCm']},
               'rotation':{'pitch':0,'yaw':0,'roll':0},'scale':{'x':1,'y':1,'z':1}})
assert call(AS,'save_assets',asset_paths=[LEVEL])
call(SC,'load_level',level_path=LEVEL)
result['savedAndReopened'] = call(SC,'get_current_level') == LEVEL
(JOB/'evidence/prop-review-placement-r6.json').write_text(json.dumps(result,indent=2))
print('R6 draft saved and reopened; previous draft untouched.')
