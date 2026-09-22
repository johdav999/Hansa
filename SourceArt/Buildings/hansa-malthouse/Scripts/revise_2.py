import bpy, math, random
from pathlib import Path
from mathutils import Vector
J=Path(__file__).resolve().parents[1];random.seed(22)
bpy.ops.wm.open_mainfile(filepath=str(J/'checkpoints/malthouse-r1.blend'))
clay=bpy.data.materials['M_MaltHouse_Clay'];clay['source']='malt-clay.png'
for n in clay.node_tree.nodes:
 if n.type=='TEX_IMAGE':n.image=bpy.data.images.load(str(J/'textures/malt-clay.png'),check_existing=True)
for o in bpy.data.collections['Roof'].objects:
 if o.type!='MESH' or not any(k in o.name for k in ['overlapping','shingle courses','tile courses']):continue
 mesh=o.data
 for i in range(0,len(mesh.vertices),10):
  if i+9>=len(mesh.vertices):continue
  v=mesh.vertices;direction=(v[i+1].co-v[i].co).normalized()*.004
  for k in [0,3,6,9]:v[i+k].co+=direction
  for k in [1,2,7,8]:v[i+k].co-=direction
  offset=Vector((random.random(),random.random()))
  for p in mesh.polygons[(i//10)*7:(i//10+1)*7]:
   for li in p.loop_indices:mesh.uv_layers.active.data[li].uv+=offset
 mesh.update()
brick=bpy.data.materials['M_MaltHouse_Brick'];oak=bpy.data.materials['M_MaltHouse_Oak'];mortar=bpy.data.materials['M_MaltHouse_Mortar']
def box(n,loc,dims,mat,coll='Timber'):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name=n;o.dimensions=dims;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 for c in list(o.users_collection):c.objects.unlink(o)
 bpy.data.collections[coll].objects.link(o);o.data.materials.append(mat);o.data.use_auto_smooth=True
 for p in o.data.polygons:
  a=max(range(3),key=lambda k:abs(p.normal[k]));axes=[k for k in range(3) if k!=a];axes.sort(key=lambda k:dims[k])
  for li in p.loop_indices:
   co=o.data.vertices[o.data.loops[li].vertex_index].co;o.data.uv_layers.active.data[li].uv=(co[axes[0]]/2.5,co[axes[1]]/2.5)
 b=o.modifiers.new('Soft edges','BEVEL');b.width=.008;b.segments=2;o.modifiers.new('Normals','WEIGHTED_NORMAL');return o
for o in list(bpy.data.collections['Masonry'].objects):
 if o.name.startswith('Timber gable infill'):
  o.data.materials.clear();o.data.materials.append(mortar)
for xx in [-4,4]:
 verts=[];faces=[]
 for row in range(35):
  z=3.67+row*.095;half=(7-z)/3.4*3
  if half<=0:continue
  u=-1-half
  while u<-1+half:
   lo=u+.006;hi=min(u+.278,-1+half)
   if hi>lo:
    p=(xx+(.03 if xx>0 else -.03),(lo+hi)/2,z+.04);dims=(.11,hi-lo,.083);idx=len(verts)
    verts.extend([(p[0]+a*dims[0]/2,p[1]+b*dims[1]/2,p[2]+c*dims[2]/2) for a,b,c in [(-1,-1,-1),(-1,-1,1),(-1,1,-1),(-1,1,1),(1,-1,-1),(1,-1,1),(1,1,-1),(1,1,1)]])
    faces.extend([tuple(idx+k for k in f) for f in [(0,4,6,2),(1,3,7,5),(0,1,5,4),(2,6,7,3),(0,2,3,1),(4,5,7,6)]])
   u+=.29
 mesh=bpy.data.meshes.new('Gable brick courses');mesh.from_pydata(verts,[],faces);mesh.update();o=bpy.data.objects.new('Gable brick courses',mesh);bpy.data.collections['Masonry'].objects.link(o);mesh.materials.append(brick);uv=mesh.uv_layers.new()
 for p in mesh.polygons:
  for li in p.loop_indices:
   co=mesh.vertices[mesh.loops[li].vertex_index].co;uv.data[li].uv=(co.y/2.5,co.z/2.5)
# Broader honest opening frames cover cut brick ends; no floating teeth around ventilation holes.
for o in bpy.data.collections['Openings'].objects:
 if 'jamb' in o.name.lower() and 'Grain door' not in o.name:
  if o.dimensions.x>o.dimensions.y:o.dimensions.y+=.15
  else:o.dimensions.x+=.15
# Grain loft hatch and lifting beam give the facade its loading function.
box('Loft loading hatch shadow',(4.12,-1,4.7),(.1,1.12,1.5),bpy.data.materials['M_MaltHouse_Recess'],'Openings')
for y in [-1.7,-.3]:box('Loft hatch jamb',(4.2,y,4.7),(.21,.17,1.8),oak,'Openings')
for z in [3.85,5.55]:box('Loft hatch lintel',(4.2,-1,z),(.24,1.58,.18),oak,'Openings')
for i in range(6):box('Loft door plank',(4.22,-1.5+i*.2,4.7),(.10,.188,1.5),oak,'Openings')
box('Grain hoist cantilever',(4.65,-1,5.78),(1.4,.2,.2),oak)
box('Hoist upright',(4.0,-1,5.75),(.22,.24,.95),oak)
S=bpy.context.scene;bpy.ops.wm.save_as_mainfile(filepath=str(J/'checkpoints/malthouse-r2.blend'));S.render.filepath=str(J/'renders/r2-hero.png');bpy.ops.render.render(write_still=True)
S.render.image_settings.file_format='JPEG';S.render.image_settings.quality=92;bpy.data.images['Render Result'].save_render(str(J/'renders/r2-hero.jpg'),scene=S)
