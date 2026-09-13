"""Tune only the seven task-owned staged props after native detail review."""
import json
from RostockTerrainSession import JOB, call

GROUP = 'editor_toolset.toolsets.'
SC = GROUP + 'scene.SceneTools'
SM = GROUP + 'static_mesh.StaticMeshTools'
AS = GROUP + 'asset.AssetTools'
LEVEL = '/Game/Hansa/Generated/Staging/CityLife_P20/L_CityLife_Review'
assert call(SC, 'get_current_level') == LEVEL
context = call('HansaEditor.HansaCityTerrainToolset', 'InspectAuthoringContext')
if isinstance(context, str):
    context = json.loads(context)
assert not context['pieRunning'] and not context['dirtyPackages'], context
records = json.loads((JOB / 'evidence/unreal-street-meshes.json').read_text())
assert len(records) == 7
changes = []
for name, record in records.items():
    mesh = record['mesh']
    expected = '/Game/Hansa/Generated/Staging/CityLife_P20/Meshes/' + name + '.' + name
    assert mesh['refPath'] == expected
    assert call(SM, 'set_lod_thresholds', mesh=mesh, thresholds=[1, .08, .025])
    assert call(AS, 'save_assets', asset_paths=[expected])
    changes.append({'mesh': expected, 'previousThresholds': [1, .3, .1],
                    'thresholds': [1, .08, .025], 'status': 'awaiting-comparison'})
(JOB / 'evidence/prop-lod-correction.json').write_text(json.dumps(changes, indent=2))
print('Seven staged meshes saved; native comparison still required.')
