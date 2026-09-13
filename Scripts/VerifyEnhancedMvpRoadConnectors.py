"""Measure native source connector profiles without altering the road master."""
import bpy,json
from pathlib import Path
repo=Path(__file__).resolve().parents[1];job=repo/'Saved/GenerationJobs/hansa-road_P18_20260908'
bpy.ops.wm.open_mainfile(filepath=str(job/'exports/HansaRoad.blend'))
geometry=json.loads((job/'evidence/geometry-r4.json').read_text())
if 'modules' in geometry:geometry=geometry['modules']
profiles={}
for name,record in geometry.items():
    ob=bpy.data.objects[name]
    for endpoint in record['sourcePorts']:
        axis=0 if endpoint[0] else 1;edge=endpoint[axis]
        profile=sorted(set((round(v.co[1-axis],5),round(v.co.z,5)) for v in ob.data.vertices if abs(v.co[axis]-edge)<.00001))
        assert profile,(name,endpoint)
        profiles[name+str(endpoint)]=profile
reference=next(iter(profiles.values()))
for name,profile in profiles.items():assert profile==reference,(name,profile,reference)
(job/'evidence/connectors.json').write_text(json.dumps({'matchedPortProfiles':len(profiles),'nativeCellMetres':4,'positionToleranceMetres':.00001,'profile':reference,'gradeContract':'common flat MVP datum; no per-cell terrain conformance claimed'},indent=2))
print('P18_CONNECTORS_MATCH',len(profiles))
