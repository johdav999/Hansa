"""Stage seven reviewed P20 street modules; reuse unchanged approved P15 materials."""
import json
from InspectEnhancedMvpCities import JOB,call
G='editor_toolset.toolsets.';AS=G+'asset.AssetTools';SM=G+'static_mesh.StaticMeshTools';SC=G+'scene.SceneTools'
ROOT='/Game/Hansa/Generated/Staging/CityLife_P20'
manifest=json.loads((JOB/'exports/export-manifest.json').read_text())
assert manifest['revision']==5
assert len(json.loads((JOB/'evidence/reimport.json').read_text()))==2
current=call(SC,'get_current_level')
assert current in ('/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP','/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/L_Lubeck_Terrain_Preview_WP')
materials={key:{'refPath':'/Game/Mesh/hansa-market/Materials/M_Market_'+key+'.M_Market_'+key} for key in ('Oak','Iron','Wicker')}
for value in materials.values():assert call(AS,'exists',path=value['refPath'])
for name in manifest['modules']:assert not call(AS,'exists',path=ROOT+'/Meshes/'+name),'Refuse overwrite '+name
records={}
for name,info in manifest['modules'].items():
    mesh=call(SM,'import_file',folder_path=ROOT+'/Meshes',asset_name=name,source_file=str(JOB/'exports'/info['fbx']),import_materials=False,import_textures=False,combine_meshes=True)[0]
    bounds=call(SM,'get_bounds',mesh=mesh)
    for i,axis in enumerate(('x','y','z')):
        coords=[p[i]*100*(-1 if axis=='y' else 1) for p in info['boundsMetres']]
        assert abs(bounds['min'][axis]-min(coords))<.2 and abs(bounds['max'][axis]-max(coords))<.2,(name,bounds)
    assert call(SM,'generate_lods',mesh=mesh,triangle_percents=[.5,.25])==3
    assert call(SM,'set_lod_thresholds',mesh=mesh,thresholds=[1,.3,.1])
    assert call(SM,'generate_convex_collisions',mesh=mesh,hull_count=4,max_hull_verts=16,hull_precision=50000)
    assert not call(SM,'is_nanite_enabled',mesh=mesh)
    slots=call(SM,'get_material_slots',mesh=mesh)
    assert len(slots)<=3
    for slot in slots:
        family=slot.removeprefix('M_CityLife_').removesuffix('_Portable')
        assert family in materials
        assert call(SM,'set_material',mesh=mesh,slot_name=slot,material=materials[family])
    assert call(AS,'save_assets',asset_paths=[mesh['refPath']])
    records[name]={'mesh':mesh,'boundsCm':bounds,'triangles':[call(SM,'get_triangle_count',mesh=mesh,lod_index=i) for i in range(3)],'materials':[materials[s.removeprefix('M_CityLife_').removesuffix('_Portable')] for s in slots], 'collision':'simple hulls only; future dressing instances must disable collision/navigation','status':'staged-awaiting-native-review'}
    (JOB/'evidence/unreal-street-meshes.json').write_text(json.dumps(records,indent=2))
    print('STAGED',name,flush=True)
assert call(SC,'get_current_level')==current
print('P20_PARTIAL_STAGING_COMPLETE_NOT_PROMOTED')
