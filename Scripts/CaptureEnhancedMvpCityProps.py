"""Native reopened prop-review captures at three gameplay distances plus material closeups."""
import base64
import json
import math
import struct
import time
import argparse
from RostockTerrainSession import JOB, call

parser = argparse.ArgumentParser()
parser.add_argument('--revision', default='r1')
parser.add_argument('--only', nargs='*')
parser.add_argument('--geometry-revision', type=int, choices=(5,6), default=5)
args = parser.parse_args()
assert args.revision.isalnum(), 'Use an alphanumeric evidence revision.'

LEVEL = '/Game/Hansa/Generated/Staging/CityLife_P20/L_CityLife_Review'
if args.geometry_revision == 6:
    LEVEL += '_r6'
assert call('editor_toolset.toolsets.scene.SceneTools','get_current_level') == LEVEL
export_dir = 'exports' if args.geometry_revision == 5 else 'exports-r6'
manifest = json.loads((JOB/export_dir/'export-manifest.json').read_text())
views = [(str(d)+'m',(0,0,70),d*100,-40,145) for d in (25,65,120)]
for name in manifest['modules']:
    x,y,z = manifest['roleOffsetsMetres'][name]
    target_z = 95 if 'Sign' in name or 'Well' in name else 45
    # Tall silhouettes require more distance in this very wide native editor viewport.
    distance = 850 if 'Sign' in name or 'Well' in name else 500
    views.append((name,(x*100,-y*100,target_z),distance,-25,145))
records = []
for tag,target,distance,pitch,yaw in views:
    if args.only and tag not in args.only:
        continue
    horizontal = distance*math.cos(math.radians(pitch))
    pose = {'location':{'x':target[0]-horizontal*math.cos(math.radians(yaw)),
                        'y':target[1]-horizontal*math.sin(math.radians(yaw)),
                        'z':target[2]-distance*math.sin(math.radians(pitch))},
            'rotation':{'pitch':pitch,'yaw':yaw,'roll':0},'scale':{'x':1,'y':1,'z':1}}
    call('EditorToolset.EditorAppToolset','SetCameraTransform',transform=pose)
    time.sleep(1)
    captured = call('EditorToolset.EditorAppToolset','CaptureViewport',captureTransform=pose,annotations=None,bShowUI=False)
    payload = base64.b64decode(captured.pop('image')['data'])
    path = JOB/'renders'/('unreal-props-'+tag+'-'+args.revision+'.png')
    assert not path.exists(), 'Preserve earlier comparison evidence: '+str(path)
    path.write_bytes(payload)
    captured.update({'tag':tag,'nativeDimensions':struct.unpack('>II',payload[16:24]),'bytes':len(payload)})
    records.append(captured)
    (JOB/('evidence/prop-native-captures-'+args.revision+'.json')).write_text(json.dumps(records,indent=2))
    print(tag,len(payload),flush=True)
