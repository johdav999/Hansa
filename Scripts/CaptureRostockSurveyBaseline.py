"""Native survey baseline only; default Landscape material is deliberately not final art."""
import base64
import json
import math
import struct
import time
from RostockTerrainSession import JOB, call

context = json.loads(call('HansaEditor.HansaCityTerrainToolset','InspectAuthoringContext'))
level = '/Game/Hansa/Generated/Staging/RostockTerrain_P31_20260908/L_Rostock_Survey_WP'
assert context['map'] == level and not context['pieRunning']
prefix = '/Game/__ExternalActors__/Hansa/Generated/Staging/RostockTerrain_P31_20260908/L_Rostock_Survey_WP/'
assert all(p == level or p.startswith(prefix) for p in context['dirtyPackages'])
# All dirty packages were just checked to belong exclusively to this draft.
if context['dirtyPackages']:
    assert call('editor_toolset.toolsets.asset.AssetTools','save_assets',asset_paths=[])
pose = {'location':{'x':0,'y':-120000,'z':1000000},
        'rotation':{'pitch':-math.degrees(math.atan2(998500,120000)),'yaw':90,'roll':0},
        'scale':{'x':1,'y':1,'z':1}}
call('EditorToolset.EditorAppToolset','SetCameraTransform',transform=pose)
time.sleep(3)
capture = call('EditorToolset.EditorAppToolset','CaptureViewport',captureTransform=pose,annotations=None,bShowUI=False)
payload = base64.b64decode(capture.pop('image')['data'])
assert payload[:8] == b'\x89PNG\r\n\x1a\n'
dimensions = struct.unpack('>II',payload[16:24])
path = JOB/'renders/rostock-survey-baseline-r2.png'
path.write_bytes(payload)
capture.update({'nativeDimensions':dimensions,'bytes':len(payload),
                'status':'survey geometry baseline; no production materials, water, or city assembly'})
(JOB/'evidence/rostock-survey-baseline-r2.json').write_text(json.dumps(capture,indent=2))
print(json.dumps(capture,indent=2))
