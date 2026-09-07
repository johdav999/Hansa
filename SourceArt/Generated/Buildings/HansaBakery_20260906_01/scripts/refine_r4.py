import bpy,math,random,pathlib,bmesh
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1];S=bpy.context.scene;random.seed(78)
def mat(n):return bpy.data.materials[n]
def box(n,loc,dim,m,c='Masonry',bev=.008):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name=n;o.dimensions=dim;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 for co in list(o.users_collection):co.objects.unlink(o)
 bpy.data.collections[c].objects.link(o);o.data.materials.append(mat(m))
 if bev:
  b=o.modifiers.new('Edge_wear','BEVEL');b.width=bev;b.segments=2;o.modifiers.new('Face_normals','WEIGHTED_NORMAL')
 return o
# correct inward-facing rear shutter assemblies by reflecting their geometry.
for o in list(bpy.data.objects):
 if o.type not in ['MESH','CURVE'] or not o.users_collection or o.users_collection[0].name=='Review':continue
 if o.type=='MESH' and len(o.data.vertices):
  points=[o.matrix_world@v.co for v in o.data.vertices];mid=sum((p.y for p in points))/len(points)
 else:mid=o.location.y
 if 6.50<mid<6.90 and o.name.startswith(('Deep_reveal','Dark_glazed_opening','Shutter','Stone_sill','Cut_stone_arch')):
  # mirror around external plane y=6.65
  o.location.y=13.3-o.location.y;o.scale.y*=-1
for o in bpy.data.objects:
 if o.name.startswith(('Rye_loaf','Flour_sack','Carved_bread')) and o.type=='MESH':
  for p in o.data.polygons:p.use_smooth=True
  b=o.modifiers.new('Organic_surface','SUBSURF');b.levels=1;b.render_levels=1
# wood grain follows timber length (local Z); bump remains restrained.
wood=mat('Weathered_Oak')
for n in wood.node_tree.nodes:
 if n.type=='VECT_MATH':n.inputs[1].default_value=(45,3,1.4)
 if n.type=='VALTORGB':
  n.color_ramp.elements[0].color=(.061,.039,.021,1);n.color_ramp.elements[1].color=(.145,.099,.057,1)
# lower plaster cloud contrast, maintaining fine lime relief
for mn in ['Lime_Plaster','Damp_Lime']:
 for n in mat(mn).node_tree.nodes:
  if n.type=='VALTORGB':
   base=mat(mn).diffuse_color[:3];n.color_ramp.elements[0].color=(*(v*.91 for v in base),1);n.color_ramp.elements[1].color=(*(v*1.04 for v in base),1)
# replace rear plain upper façade with individual shallow bricks
verts=[];faces=[];indices=[]
def brick(loc,dim,idx):
 x,y,z=loc;a,b,c=[q/2 for q in dim];off=len(verts)
 verts.extend([(x+dx*a,y+dy*b,z+dz*c) for dx,dy,dz in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]])
 for f in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]:faces.append(tuple(off+i for i in f));indices.append(idx)
for r in range(74):
 z=6.23+r*.112
 for j in range(35):
  x=-4.45+j*.262+(r%2)*.13
  if x>4.45 or z>14.26-abs(x)*1.05:continue
  if any(abs(x-xx)<.65 and 6.9<z<8.75 for xx in [-2,1]):continue
  brick((x,6.515,z),(.247,.08,.098),random.randrange(3))
for r in range(42):
 z=3.5+r*.174
 for side in [-1,1]:
  for j in range(3):
   brick((2.9+side*.44,7.2+j*.3,z),(.075,.28,.158),random.randrange(3))
   brick((2.62+j*.28,7.5+side*.44,z),(.26,.075,.158),random.randrange(3))
me=bpy.data.meshes.new('Rear_brickwork');me.from_pydata(verts,[],faces);me.update();o=bpy.data.objects.new('Rear_and_chimney_bricks',me);bpy.data.collections['Masonry'].objects.link(o)
for i in range(3):me.materials.append(mat('Brick_'+str(i)))
for p,i in zip(me.polygons,indices):p.material_index=i
b=o.modifiers.new('Mortar_edge_wear','BEVEL');b.width=.005;b.segments=2;o.modifiers.new('Weighted','WEIGHTED_NORMAL')
# Bakehouse lean-to tiles, each row overlaps downslope.
for r in range(16):
 for col in range(20):
  x=-.52+col*.265;y=6.43+r*.278;z=4.4-(y-6.43)*.161
  o=box('Bakehouse_tile',(x,y,z),(.257,.35,.045),'Clay_'+str(random.randrange(3)),'Roof');o.rotation_euler[0]=-.16
# Sign has visible carved loaves on both faces.
o=bpy.data.objects.get('Carved_bread_emblem');n=o.copy();n.data=o.data.copy();n.name='Carved_bread_emblem_reverse';n.location.x=3.855;bpy.data.collections['Bakery'].objects.link(n)
# crown has an actual open flue instead of a solid cap.
for name in ['Chimney_crown.002']:
 o=bpy.data.objects.get(name)
 if o:bpy.data.objects.remove(o,do_unlink=True)
for x,y,dx,dy in [(2.9,7.03,1.04,.15),(2.9,7.97,1.04,.15),(2.43,7.5,.15,.8),(3.37,7.5,.15,.8)]:box('Chimney_open_coping',(x,y,10.83),(dx,dy,.19),'Brick_1')
# weathered lower oven surround and bread scores as physical shallow marks
for o in list(bpy.data.objects):
 if o.name.startswith('Rye_loaf'):
  for off in [-.06,.035]:
   cv=bpy.data.curves.new('Bread_score','CURVE');cv.dimensions='3D';cv.bevel_depth=.008;cv.bevel_resolution=2;sp=cv.splines.new('POLY');sp.points.add(6)
   for k,p in enumerate(sp.points):
    t=(k-3)/3;p.co=(o.location.x+t*.10,o.location.y+off+t*.035,o.location.z+.097-.03*t*t,1)
   ob=bpy.data.objects.new('Bread_score',cv);bpy.data.collections['Bakery'].objects.link(ob);cv.materials.append(mat('Flour_Linen'))
S.camera=bpy.data.objects['Hero'];bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints'/'bakery_r4.blend'))
for name in ['Hero','Rear','Detail_Shop','Detail_Gable','Detail_Roof']:
 S.camera=bpy.data.objects[name];S.render.filepath=str(P/'renders'/('r4_'+name+'.png'));bpy.ops.render.render(write_still=True)
