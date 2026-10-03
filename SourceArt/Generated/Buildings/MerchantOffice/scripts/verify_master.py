import bpy
import json
from pathlib import Path

images=[i for i in bpy.data.images if i.source=='FILE']
report={'blend':bpy.data.filepath,'images':[{'name':i.name,'packed':bool(i.packed_file),'size':list(i.size)} for i in images]}
assert len(images)>=3,report
assert all(i.packed_file for i in images),report
print('PACKED_MASTER_VERIFIED',json.dumps(report))
