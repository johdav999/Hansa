"""Place the seven staged meshes at authored metre-to-centimetre scale in the isolated review."""
import json
from RostockTerrainSession import JOB, call

G = 'editor_toolset.toolsets.'
LEVEL = '/Game/Hansa/Generated/Staging/CityLife_P20/L_CityLife_Review'
assert call(G+'scene.SceneTools','get_current_level') == LEVEL
actors = call(G+'scene.SceneTools','find_actors',name='',tag='',collision_channels=[])
floor = next(a for a in actors if a['refPath'].endswith(':PersistentLevel.StaticMeshActor_1'))
bounds = call(G+'actor.ActorTools','get_actor_bounds',actor=floor)
assert bounds['isValid']
height = bounds['max']['z']
old = [a for a in actors if a['refPath'].endswith(':PersistentLevel.BP_Market_Review_C_0')]
assert len(old) == 1, 'Review template already modified; inspect before retry'
assert old[0]['refPath'].startswith(LEVEL+'.')
assert call(G+'scene.SceneTools','remove_from_scene',actor=old[0])
manifest = json.loads((JOB/'exports/export-manifest.json').read_text())
meshes = json.loads((JOB/'evidence/unreal-street-meshes.json').read_text())
result = {'level':LEVEL,'floorTopCm':height,'actors':{}}
for name,info in meshes.items():
    x,y,z = manifest['roleOffsetsMetres'][name]
    actor = call(G+'scene.SceneTools','add_to_scene_from_asset',asset_path=info['mesh']['refPath'],name='P20_'+name,
                 xform={'location':{'x':x*100,'y':-y*100,'z':height},
                        'rotation':{'pitch':0,'yaw':0,'roll':0},'scale':{'x':1,'y':1,'z':1}})
    assert actor
    result['actors'][name] = actor
    (JOB/'evidence/prop-review-placement.json').write_text(json.dumps(result,indent=2))
assert call(G+'asset.AssetTools','save_assets',asset_paths=[LEVEL])
call(G+'scene.SceneTools','load_level',level_path=LEVEL)
assert call(G+'scene.SceneTools','get_current_level') == LEVEL
result['savedAndReopened'] = True
(JOB/'evidence/prop-review-placement.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
