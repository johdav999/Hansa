"""Verify the delivered packed master and render a native-size turntable sequence."""
from pathlib import Path
import bpy, math, json
from mathutils import Vector
REPO=Path(__file__).resolve().parents[1]
JOB=REPO/'Saved/GenerationJobs/hansa-harbor_P17_20260908'
bpy.ops.wm.open_mainfile(filepath=str(JOB/'exports/HansaHarbor.blend'))
images=[im for im in bpy.data.images if im.source=='FILE']
assert images and all(im.packed_file for im in images), 'Delivered master must be self-contained'
s=bpy.context.scene
s.render.resolution_x=1280;s.render.resolution_y=720;s.render.resolution_percentage=100
s.render.image_settings.file_format='PNG'
camera=bpy.data.objects.get('HarborWhole')
assert camera
s.camera=camera
out=JOB/'renders/turntable';out.mkdir(parents=True,exist_ok=True)
for i in range(8):
    a=math.radians(-50+i*45)
    camera.location=(30*math.cos(a),30*math.sin(a),18)
    camera.rotation_euler=(Vector((0,0,-.1))-camera.location).to_track_quat('-Z','Y').to_euler()
    s.render.filepath=str(out/f'{i:02d}.png')
    bpy.ops.render.render(write_still=True)
(JOB/'evidence/packed-master-review.json').write_text(json.dumps({'master':'exports/HansaHarbor.blend','packedImages':len(images),'allFileImagesPacked':True,'turntableFrames':8,'nativeDimensions':[1280,720]},indent=2))
