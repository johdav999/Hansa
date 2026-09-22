import bpy, math, random
from mathutils import Vector
from pathlib import Path
J=Path(__file__).resolve().parents[1];random.seed(21)
bpy.ops.wm.open_mainfile(filepath=str(J/'checkpoints/malthouse-r0.blend'))
for o in bpy.data.objects:
 if o.type=='MESH':o.data.use_auto_smooth=True
clay=bpy.data.materials['M_MaltHouse_Clay']; C=bpy.data.collections['Roof']
def tiled_quad(n,A,B,D,E):
 A,B,D,E=map(Vector,[A,B,D,E]); verts=[]; faces=[]
 count=math.ceil(max((D-A).length,(E-B).length)/.28)
 for row in range(count):
  t=row/count;t1=min(1,(row+1.26)/count);L=A.lerp(D,t);R=B.lerp(E,t);L1=A.lerp(D,t1);R1=B.lerp(E,t1);cols=math.ceil((R-L).length/.235)
  for col in range(cols):
   u=col/cols;u1=(col+1)/cols;p=L.lerp(R,u);q=L.lerp(R,u1);r=L1.lerp(R1,u1);s=L1.lerp(R1,u)
   normal=(q-p).cross(s-p).normalized()
   if normal.z<0:normal=-normal
   p+=normal*(.037+random.uniform(-.002,.002));q+=normal*.037;r+=normal*.006;s+=normal*.006
   idx=len(verts);mid=(p+q)/2+normal*.012;mid2=(s+r)/2+normal*.012
   verts.extend([tuple(v) for v in [p,q,r,s,mid,mid2,p-normal*.027,q-normal*.027,r-normal*.027,s-normal*.027]])
   faces.extend([tuple(idx+k for k in f) for f in [(0,4,5,3),(4,1,2,5),(0,6,7,1),(1,7,8,2),(2,8,9,3),(3,9,6,0),(6,9,8,7)]])
 mesh=bpy.data.meshes.new(n);mesh.from_pydata(verts,[],faces);mesh.update();o=bpy.data.objects.new(n,mesh);C.objects.link(o);mesh.materials.append(clay)
 uv=mesh.uv_layers.new()
 for p in mesh.polygons:
  a=max(range(3),key=lambda k:abs(p.normal[k]));axes=[k for k in range(3) if k!=a]
  for li in p.loop_indices:
   co=mesh.vertices[mesh.loops[li].vertex_index].co;uv.data[li].uv=(co[axes[0]]/2.5,co[axes[1]]/2.5)
 return o
tiled_quad('West overlapping clay tiles',(-4.32,-4.37,3.53),(4.32,-4.37,3.53),(-4.32,-1,7.13),(4.32,-1,7.13))
tiled_quad('East overlapping clay tiles',(4.32,2.37,3.53),(-4.32,2.37,3.53),(4.32,-1,7.13),(-4.32,-1,7.13))
for x in [i*.32-4.16 for i in range(27)]:
 bpy.ops.mesh.primitive_cylinder_add(vertices=12,radius=.115,depth=.35,location=(x,-1,7.13),rotation=(0,math.pi/2,0));o=bpy.context.object;o.name='Clay ridge cap';o.data.materials.append(clay)
 for c in list(o.users_collection):c.objects.unlink(o)
 C.objects.link(o)
lower=[(-4.07,1.83,4.18),(-.13,1.83,4.18),(-.13,5.37,4.18),(-4.07,5.37,4.18)];upper=[(-2.67,3.03,7.58),(-1.53,3.03,7.58),(-1.53,4.17,7.58),(-2.67,4.17,7.58)]
for i in range(4):tiled_quad('Kiln clay shingle courses',lower[i],lower[(i+1)%4],upper[i],upper[(i+1)%4])
base=[(-2.97,2.73,8.46),(-1.23,2.73,8.46),(-1.23,4.47,8.46),(-2.97,4.47,8.46)]
for i in range(4):tiled_quad('Cowl tile courses',base[i],base[(i+1)%4],(-2.105,3.6,9.18),(-2.095,3.6,9.18))
S=bpy.context.scene;bpy.ops.wm.save_as_mainfile(filepath=str(J/'checkpoints/malthouse-r1.blend'));S.render.filepath=str(J/'renders/r1-hero.png');bpy.ops.render.render(write_still=True)
S.render.image_settings.file_format='JPEG';S.render.image_settings.quality=92;bpy.data.images['Render Result'].save_render(str(J/'renders/r1-hero.jpg'),scene=S)
