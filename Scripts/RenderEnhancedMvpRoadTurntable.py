"""Native-resolution front, side and rear QA of the reopened packed P18 master."""
import bpy,math,json
from pathlib import Path
from mathutils import Vector
repo=Path(__file__).resolve().parents[1];job=repo/'Saved/GenerationJobs/hansa-road_P18_20260908'
bpy.ops.wm.open_mainfile(filepath=str(job/'exports/HansaRoad.blend'))
assert all(i.packed_file for i in bpy.data.images if i.source=='FILE')
s=bpy.context.scene;s.render.resolution_x=1280;s.render.resolution_y=720;s.render.resolution_percentage=100
if not s.camera:
    bpy.ops.object.camera_add();s.camera=bpy.context.object;s.camera.data.lens=45
s.cycles.samples=24
for ob in s.objects:
    if ob.type=='MESH' and not (ob.name.startswith('SM_HansaRoad_') or ob.name=='ReviewGround'):ob.hide_render=True
for name,location in [('front',(0,-24,20)),('side',(24,0,20)),('rear',(0,24,20))]:
    s.camera.location=location;s.camera.rotation_euler=(Vector((0,0,0))-s.camera.location).to_track_quat('-Z','Y').to_euler()
    s.render.filepath=str(job/'renders'/('turntable-'+name+'.png'));bpy.ops.render.render(write_still=True)
(job/'evidence/packed-master-reopened.json').write_text(json.dumps({'packedImagesVerified':True,'nativeRenderDimensions':[1280,720],'views':['front','side','rear']}))
