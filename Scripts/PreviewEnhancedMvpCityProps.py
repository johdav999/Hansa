"""Create an isolated P20 review sibling; never modify the source review or city maps."""
import json
from RostockTerrainSession import JOB, call

G = 'editor_toolset.toolsets.'
LEVEL = '/Game/Hansa/Generated/Staging/CityLife_P20/L_CityLife_Review'
SOURCE = '/Game/Hansa/Generated/Staging/Market_P15/L_Market_Review'
context = json.loads(call('HansaEditor.HansaCityTerrainToolset','InspectAuthoringContext'))
assert not context['pieRunning'] and not context['dirtyPackages'], context
assert call(G+'asset.AssetTools','exists',path=SOURCE)
assert not call(G+'asset.AssetTools','exists',path=LEVEL), 'Existing review must be inspected; no overwrite'
assert call(G+'asset.AssetTools','duplicate',path=SOURCE,new_path=LEVEL)
assert call(G+'asset.AssetTools','save_assets',asset_paths=[LEVEL])
call(G+'scene.SceneTools','load_level',level_path=LEVEL)
actors = call(G+'scene.SceneTools','find_actors',name='',tag='',collision_channels=[])
assert all(a['refPath'].startswith(LEVEL+'.') for a in actors)
(JOB/'evidence/prop-review-template-actors.json').write_text(json.dumps(actors,indent=2))
print(json.dumps(actors,indent=2))
