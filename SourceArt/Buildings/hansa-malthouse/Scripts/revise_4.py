import bpy,bmesh,math
from pathlib import Path
from mathutils import Vector
J=Path(__file__).resolve().parents[1];bpy.ops.wm.open_mainfile(filepath=str(J/'checkpoints/malthouse-r3.blend'))
oak=bpy.data.materials['M_MaltHouse_Oak'];dark=bpy.data.materials['M_MaltHouse_Recess']
def box(n,p,d,m):
 bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.name=n;o.dimensions=d;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 for c in list(o.users_collection):c.objects.unlink(o)
 bpy.data.collections['Openings'].objects.link(o);o.data.materials.append(m);o.data.use_auto_smooth=True
 for poly in o.data.polygons:
  a=max(range(3),key=lambda k:abs(poly.normal[k]));axes=[k for k in range(3) if k!=a];axes.sort(key=lambda k:d[k])
  for li in poly.loop_indices:
   co=o.data.vertices[o.data.loops[li].vertex_index].co;o.data.uv_layers.active.data[li].uv=(co[axes[0]]/2.5,co[axes[1]]/2.5)
 b=o.modifiers.new('Worn edges','BEVEL');b.width=.009;b.segments=2;o.modifiers.new('Normals','WEIGHTED_NORMAL');return o
# Courtyard inspection exposed forward-facing lime backing on the negative-X-positioned kiln wall.
for o in bpy.data.collections['Masonry'].objects:
 if o.name.startswith('Kiln masonry_limebed') and -.4<o.location.x<0:o.location.x-=.15
box('Courtyard service door recess',(1.55,1.91,1.25),(1.1,.1,2.2),dark)
for x in [.87,2.23]:box('Service jamb',(x,2.07,1.22),(.22,.23,2.5),oak)
box('Service lintel',(1.55,2.07,2.42),(1.57,.24,.22),oak)
for i in range(6):box('Service door boards',(1.08+i*.19,2.025,1.25),(.178,.085,2.18),oak)
for z in [.55,1.95]:box('Service iron strap',(1.55,2.085,z),(.95,.028,.065),bpy.data.materials['M_MaltHouse_Iron'])
# Recalculate closed-mesh face orientation; earlier hand-built bricks were inward wound.
for o in bpy.data.objects:
 if o.type!='MESH':continue
 bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free();o.data.update()
S=bpy.context.scene
def render(n,loc,target=(0,0,3.6),scale=16.8):
 cam=S.camera;cam.location=loc;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=scale
 S.render.image_settings.file_format='PNG';S.render.filepath=str(J/'renders'/('r4-'+n+'.png'));bpy.ops.render.render(write_still=True);S.render.image_settings.file_format='JPEG';S.render.image_settings.quality=93;bpy.data.images['Render Result'].save_render(str(J/'renders'/('r4-'+n+'.jpg')),scene=S)
render('hero',(16,-18,13));render('courtyard',(15,18,12));render('rear',(-16,18,12));render('roof-detail',(8,-9,10),(0,-1.5,5.6),6.2);render('door-detail',(12,-8,5),(4,-1.5,2.2),4.7)
S.camera.location=(16,-18,13);S.camera.rotation_euler=(Vector((0,0,3.6))-S.camera.location).to_track_quat('-Z','Y').to_euler();S.camera.data.ortho_scale=16.8;S.render.image_settings.file_format='PNG';bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(J/'checkpoints/malthouse-r4.blend'))
