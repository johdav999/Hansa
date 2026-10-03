import bpy
import json
import sys
from pathlib import Path
from mathutils import Vector

root=next(p for p in Path(__file__).resolve().parents if (p/'Hansa.uproject').exists())
job=root/'Saved/GenerationJobs/merchant-office-hausbaumhaus_20260930'
kind=sys.argv[-1]
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
if kind=='glb':bpy.ops.import_scene.gltf(filepath=str(job/'SM_MerchantOffice_Hausbaumhaus.glb'))
elif kind=='fbx':bpy.ops.import_scene.fbx(filepath=str(job/'SM_MerchantOffice_Hausbaumhaus.fbx'))
else:raise ValueError(kind)
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
assert meshes
pts=[o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
lo=[min(p[i] for p in pts) for i in range(3)];hi=[max(p[i] for p in pts) for i in range(3)]
materials=sorted({s.name for o in meshes for s in o.data.materials if s})
images=sorted({n.image.name for m in bpy.data.materials if m.use_nodes for n in m.node_tree.nodes if n.type=='TEX_IMAGE' and n.image})
result={'format':kind,'mesh_count':len(meshes),'bounds_min':lo,'bounds_max':hi,'dimensions':[hi[i]-lo[i] for i in range(3)],'materials':materials,'images':images,'missing_uv':[o.name for o in meshes if not o.data.uv_layers]}
assert 6.8 < result['dimensions'][0] < 7.5,result
assert 6.1 < result['dimensions'][1] < 7.5,result
assert 12.0 < result['dimensions'][2] < 12.8,result
assert not result['missing_uv'],result
assert len(materials)>=7,result
(job/('verify-'+kind+'.json')).write_text(json.dumps(result,indent=2),encoding='utf8')
# Inspect the delivered interchange materials in an entirely new scene context.
bpy.ops.object.camera_add(location=(13,-18,13));camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,5.7))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=19;bpy.context.scene.camera=camera
bpy.ops.object.light_add(type='AREA',location=(-8,-10,17));key=bpy.context.object
key.data.energy=3600;key.data.shape='DISK';key.data.size=9
key.rotation_euler=(Vector((0,0,5))-key.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.light_add(type='AREA',location=(9,3,12));fill=bpy.context.object
fill.data.energy=2200;fill.data.shape='DISK';fill.data.size=8
fill.rotation_euler=(Vector((0,0,5))-fill.location).to_track_quat('-Z','Y').to_euler()
bpy.context.scene.world.color=(.65,.68,.72)
bpy.context.scene.render.engine='BLENDER_EEVEE'
bpy.context.scene.eevee.use_gtao=True
bpy.context.scene.render.resolution_x=800;bpy.context.scene.render.resolution_y=800
bpy.context.scene.render.resolution_percentage=100
bpy.context.scene.view_settings.view_transform='Standard'
bpy.context.scene.view_settings.look='Medium High Contrast'
bpy.context.scene.render.filepath=str(job/('reimport-'+kind+'.png'))
bpy.ops.render.render(write_still=True)
print('VERIFIED',json.dumps(result))
