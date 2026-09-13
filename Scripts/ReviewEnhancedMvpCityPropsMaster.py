"""Reopen the packed master and render full silhouettes from three directions."""
import bpy
import json
from pathlib import Path
from mathutils import Vector

REPO = Path(__file__).resolve().parents[1]
JOB = REPO / 'Saved/GenerationJobs/city-life_P20_20260908'
bpy.ops.wm.open_mainfile(filepath=str(JOB / 'exports/HansaCityProps.blend'))
images = [image for image in bpy.data.images if image.source == 'FILE']
assert images and all(image.packed_file for image in images)
scene = bpy.context.scene
scene.render.resolution_x = 1280
scene.render.resolution_y = 720
scene.render.resolution_percentage = 100
scene.cycles.samples = 24
camera = scene.camera
if camera is None:
    bpy.ops.object.camera_add()
    camera = bpy.context.object
    scene.camera = camera
camera.data.lens = 45
views = [('front',(0,-22,12)),('side',(22,0,12)),('rear',(0,22,12))]
for tag,position in views:
    camera.location = position
    camera.rotation_euler = (Vector((0,0,1))-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath = str(JOB / 'renders' / ('master-'+tag+'.png'))
    bpy.ops.render.render(write_still=True)
(JOB / 'evidence/packed-master-reopened.json').write_text(json.dumps({
    'packedImages':len(images), 'dimensions':[1280,720], 'views':[v[0] for v in views],
    'status':'rendered-awaiting-pixel-inspection', 'notAnimation':True}, indent=2))
