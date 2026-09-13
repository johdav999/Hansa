"""Native rendered four-repeat swatches under raking neutral light."""
import bpy
from pathlib import Path
from mathutils import Vector
JOB=Path(__file__).resolve().parents[1]/'Saved/GenerationJobs/hansa-vehicles_P19_20260908'
bpy.ops.wm.open_mainfile(filepath=str(JOB/'exports/HansaVehicles.blend'))
scene=bpy.context.scene
for ob in bpy.data.objects:ob.hide_render=True
scene.cycles.samples=32;scene.render.resolution_x=1280;scene.render.resolution_y=720;scene.render.resolution_percentage=100
bpy.ops.mesh.primitive_plane_add(size=4);plane=bpy.context.object
for uv in plane.data.uv_layers.active.data:uv.uv*=2
bpy.ops.object.light_add(type='AREA',location=(0,-3,4));light=bpy.context.object;light.data.energy=500;light.data.size=2
light.rotation_euler=(Vector((0,0,0))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(0,-3.5,5));cam=bpy.context.object;cam.data.type='ORTHO';cam.data.ortho_scale=5
cam.rotation_euler=(-cam.location).to_track_quat('-Z','Y').to_euler();scene.camera=cam
for name in ('Oak','TarredOak','Linen','Canvas','Hemp','Iron'):
    plane.data.materials.clear();plane.data.materials.append(bpy.data.materials['M_Vehicle_'+name+'_Portable'])
    scene.render.filepath=str(JOB/'renders'/('swatch-'+name+'.png'));bpy.ops.render.render(write_still=True)
print('P19_SWATCHES_RENDERED')
