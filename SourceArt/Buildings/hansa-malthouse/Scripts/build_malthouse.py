import bpy, math, random, json, sys
from mathutils import Vector
from pathlib import Path
J=Path(__file__).resolve().parents[1]
REV=int(sys.argv[sys.argv.index('--')+1]) if '--' in sys.argv else 0
random.seed(1409)
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
S=bpy.context.scene; S.unit_settings.system='METRIC'; S.unit_settings.scale_length=1
S.render.engine='BLENDER_EEVEE'; S.eevee.use_gtao=True; S.eevee.gtao_distance=1.2; S.eevee.gtao_factor=1.1
S.eevee.taa_render_samples=64; S.render.resolution_x=1200; S.render.resolution_y=1000; S.render.resolution_percentage=100
S.render.image_settings.file_format='PNG'; S.view_settings.view_transform='Filmic'; S.view_settings.look='Medium High Contrast'; S.view_settings.exposure=0
C={}
for n in ['Masonry','Timber','Roof','Openings','Kiln','Workyard','Review']:
 c=bpy.data.collections.new(n); S.collection.children.link(c); C[n]=c
def material(n,col,rough=.8,tex=None,metal=0):
 m=bpy.data.materials.new(n); m.use_nodes=True; p=m.node_tree.nodes.get('Principled BSDF'); p.inputs['Base Color'].default_value=(*col,1); p.inputs['Roughness'].default_value=rough; p.inputs['Metallic'].default_value=metal
 m.diffuse_color=(*col,1); m['roughness_mean']=rough; m['metallic']=metal; m['extent_m']=2.5
 if tex:
  t=m.node_tree.nodes.new('ShaderNodeTexImage'); t.image=bpy.data.images.load(str(J/'textures'/tex),check_existing=True); m.node_tree.links.new(t.outputs['Color'],p.inputs['Base Color']); m['source']=tex
 noise=m.node_tree.nodes.new('ShaderNodeTexNoise'); noise.inputs['Scale'].default_value=170 if tex!='oak.png' else 95; noise.inputs['Detail'].default_value=2
 uv=m.node_tree.nodes.new('ShaderNodeTexCoord'); m.node_tree.links.new(uv.outputs['UV'],noise.inputs['Vector'])
 bump=m.node_tree.nodes.new('ShaderNodeBump'); bump.inputs['Distance'].default_value=.002; bump.inputs['Strength'].default_value=.22; m.node_tree.links.new(noise.outputs['Fac'],bump.inputs['Height']); m.node_tree.links.new(bump.outputs['Normal'],p.inputs['Normal'])
 ramp=m.node_tree.nodes.new('ShaderNodeMapRange'); ramp.inputs['From Min'].default_value=0; ramp.inputs['From Max'].default_value=1; ramp.inputs['To Min'].default_value=max(.05,rough-.05); ramp.inputs['To Max'].default_value=min(1,rough+.05); m.node_tree.links.new(noise.outputs['Fac'],ramp.inputs['Value']); m.node_tree.links.new(ramp.outputs['Result'],p.inputs['Roughness'])
 return m
brick=material('M_MaltHouse_Brick',(.36,.12,.065),.86,'malt-brick.png')
oak=material('M_MaltHouse_Oak',(.16,.09,.045),.79,'oak.png')
clay=material('M_MaltHouse_Clay',(.38,.13,.055),.88,'roof.png')
mortar=material('M_MaltHouse_Mortar',(.40,.36,.28),.93)
iron=material('M_MaltHouse_Iron',(.035,.041,.043),.6,metal=.78)
cloth=material('M_MaltHouse_Sacking',(.48,.38,.23),.96)
dark=material('M_MaltHouse_Recess',(.022,.025,.022),.96)
ground=material('ReviewGround',(.16,.18,.18),.96)
buffers={}
def geo(n,verts,faces,mat,coll='Masonry',uv=None):
 mesh=bpy.data.meshes.new(n); mesh.from_pydata(verts,[],faces); mesh.update(); o=bpy.data.objects.new(n,mesh); C[coll].objects.link(o); mesh.materials.append(mat)
 layer=mesh.uv_layers.new(name='UVMap')
 for poly in mesh.polygons:
  axis=max(range(3),key=lambda a:abs(poly.normal[a])); axes=[a for a in range(3) if a!=axis]
  for li in poly.loop_indices:
   co=mesh.vertices[mesh.loops[li].vertex_index].co; layer.data[li].uv=(co[axes[0]]/2.5,co[axes[1]]/2.5)
 return o
def cube(n,loc,dim,mat,coll='Masonry',bevel=0,rot=None):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc); o=bpy.context.object; o.name=n; o.dimensions=dim; bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 for c in list(o.users_collection):c.objects.unlink(o)
 C[coll].objects.link(o); o.data.materials.append(mat)
 if rot:o.rotation_euler=rot
 # Project face UVs at 2.5 m / repeat; timber longitudinal grain follows longest member axis.
 layer=o.data.uv_layers.active
 for p in o.data.polygons:
  a=max(range(3),key=lambda k:abs(p.normal[k])); axes=[k for k in range(3) if k!=a]
  if mat==oak:axes.sort(key=lambda k:dim[k])
  for li in p.loop_indices:
   co=o.data.vertices[o.data.loops[li].vertex_index].co; layer.data[li].uv=(co[axes[0]]/2.5,co[axes[1]]/2.5)
 if bevel:
  b=o.modifiers.new('Rounded handmade edges','BEVEL'); b.width=bevel; b.segments=2
  o.modifiers.new('Weighted normals','WEIGHTED_NORMAL')
 return o
def beam(n,a,b,w=.15,d=None,mat=oak,coll='Timber'):
 a,b=Vector(a),Vector(b); o=cube(n,(a+b)/2,(w,d or w,(b-a).length),mat,coll,.012); o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler(); return o
def brickwall(n,axis,pos,u0,u1,z0,z1,holes=[]):
 # Individual structural brick faces with 12 mm lime joints, variable firing at whole-brick level.
 verts=[]; faces=[]
 for row in range(math.ceil((z1-z0)/.095)):
  z=z0+row*.095; h=min(.083,z1-z)
  if h<=0:continue
  u=u0-(.145 if row%2 else 0)
  while u<u1:
   lo=max(u,u0); hi=min(u+.278,u1); mid=(lo+hi)/2
   if hi>lo and not any(lo<hx1 and hi>hx0 and z<hz1 and z+h>hz0 for hx0,hx1,hz0,hz1 in holes):
    depth=.13+random.uniform(-.009,.009); p=[pos,mid,z+h/2] if axis==0 else [mid,pos,z+h/2]; dims=[depth,hi-lo,h] if axis==0 else [hi-lo,depth,h]
    idx=len(verts)
    verts.extend([(p[0]+a*dims[0]/2,p[1]+b*dims[1]/2,p[2]+c*dims[2]/2) for a,b,c in [(-1,-1,-1),(-1,-1,1),(-1,1,-1),(-1,1,1),(1,-1,-1),(1,-1,1),(1,1,-1),(1,1,1)]])
    faces.extend([tuple(idx+k for k in f) for f in [(0,4,6,2),(1,3,7,5),(0,1,5,4),(2,6,7,3),(0,2,3,1),(4,5,7,6)]])
   u+=.29
 o=geo(n,verts,faces,brick); b=o.modifiers.new('Brick arrises','BEVEL'); b.width=.004; b.segments=1; o.modifiers.new('Corner normals','WEIGHTED_NORMAL')
 # Backing follows wall segments so openings are genuine recesses.
 zcuts=sorted(set([z0,z1]+[v for h in holes for v in h[2:]])); ucuts=sorted(set([u0,u1]+[v for h in holes for v in h[:2]]))
 for a,b in zip(ucuts,ucuts[1:]):
  for c,d in zip(zcuts,zcuts[1:]):
   if a<u0 or b>u1 or c<z0 or d>z1:continue
   if any(h[0]<(a+b)/2<h[1] and h[2]<(c+d)/2<h[3] for h in holes):continue
   loc=[pos-(.065 if pos>0 else -.065),(a+b)/2,(c+d)/2] if axis==0 else [(a+b)/2,pos-(.065 if pos>0 else -.065),(c+d)/2]
   dim=[.14,b-a,d-c] if axis==0 else [b-a,.14,d-c]
   cube(n+'_limebed',loc,dim,mortar)
 return o
def shutter(n,x,y,z,w=1,h=.75,side=False):
 # Built along front (+X); side version rotates the complete assembly.
 added=[]
 def q(name,loc,dim,mat=oak):
  if side:loc=(y+loc[1],x-loc[0],loc[2]); dim=(dim[1],dim[0],dim[2])
  else:loc=(x+loc[0],y+loc[1],loc[2])
  o=cube(n+name,loc,dim,mat,'Openings',.009); added.append(o)
 q('reveal',(-.13,0,z),(.09,w,h),dark)
 for yy in [-w/2-.075,w/2+.075]:q('jamb',(.025,yy,z),(.2,.13,h+.23))
 for zz in [z-h/2-.065,z+h/2+.065]:q('lintel',(.025,0,zz),(.21,w+.26,.13))
 for i in range(5):q('louvre',(-.025,0,z-h/2+.08+i*(h-.08)/5),(.13,w,.085))
 return added
# Main germination hall, 8.0 m long / 6.0 m wide, 3.6 m eaves.
front_holes=[(-2.1,-.1,.15,2.6),(.55,1.5,1.75,2.55),(-3.45,-2.65,1.8,2.5)]
brickwall('Front bonded brick',0,4,-4,2,.15,3.6,front_holes)
brickwall('Rear bonded brick',0,-4,-4,2,.15,3.6,[(-1.6,-.5,1.6,2.35)])
sideholes=[(x-.5,x+.5,1.65,2.4) for x in [-2.6,0,2.6]]
brickwall('Roadside ventilation wall',1,-4,-4,4,.15,3.6,sideholes)
brickwall('Courtyard wall',1,2,-4,4,.15,3.6,[(1,2.1,.15,2.35)])
cube('Brick foundation',(0,-1,.075),(8.15,6.15,.15),brick,bevel=.015)
cube('Germination floor',(0,-1,.14),(7.8,5.8,.1),mortar)
for xx in [-4,4]:
 geo('Timber gable infill',[(xx,-4,3.6),(xx,2,3.6),(xx,-1,7)],[(0,1,2)],brick)
 for y in [-4,2]:beam('Gable rafter',(xx,y,3.6),(xx,-1,7),.19)
 beam('Gable tie',(xx,-4,3.65),(xx,2,3.65),.22)
 beam('King post',(xx,-1,3.6),(xx,-1,7),.18)
 for yy in [-2.5,.5]:beam('Gable stud',(xx,yy,3.6),(xx,yy,5.3),.16)
 for yy in [-4,2]:beam('Corner post',(xx,yy,.18),(xx,yy,3.6),.18)
for y in [-4,2]:beam('Eave plate',(-4.1,y,3.6),(4.1,y,3.6),.22)
for y,z,w,h in [(-3.05,2.15,.8,.7),(1.025,2.15,.95,.8)]:shutter('Front air shutter',4,y,z,w,h)
shutter('Rear air shutter',-4,-1.05,1.98,1.1,.75)
for x in [-2.6,0,2.6]:shutter('Malting floor vent',-4,x,2.025,1,.75,True)
for yy in [-2.15,-.05]:cube('Grain door jamb',(4.03,yy,1.4),(.25,.17,2.6),oak,'Openings',.015)
cube('Door lintel',(4.03,-1.1,2.64),(.28,2.35,.24),oak,'Openings',.02)
cube('Door recess',(3.76,-1.1,1.4),(.12,2,2.4),dark,'Openings')
for k in range(10):cube('Hand hewn grain door',(3.95,-2.04+k*.207,1.37),(.09,.197,2.4),oak,'Openings',.007)
for z in [.65,2.0]:
 for y in [-1.62,-.58]:cube('Iron strap hinge',(4.025,y,z),(.032,.82,.065),iron,'Openings',.008)
cube('Delivery threshold',(4.25,-1.1,.1),(.6,2.5,.2),mortar,bevel=.025)
def roofpanel(n,xa,xb,ya,yb,za,zb,mat=clay):
 return geo(n,[(xa,ya,za),(xb,ya,za),(xb,yb,zb),(xa,yb,zb),(xa,ya,za-.08),(xb,ya,za-.08),(xb,yb,zb-.08),(xa,yb,zb-.08)],[(0,1,2,3),(4,7,6,5),(0,4,5,1),(1,5,6,2),(2,6,7,3),(3,7,4,0)],mat,'Roof')
roofpanel('West clay roof',-4.3,4.3,-4.35,-1,3.5,7.1)
roofpanel('East clay roof',-4.3,4.3,-1,2.35,7.1,3.5)
# Squared brick kiln annex; timber cowl is an inferred game-readable vent, not a measured reconstruction.
cube('Kiln foundation',(-2.1,3.55,.12),(3.55,3.15,.24),brick,bevel=.02)
for axis,pos,u0,u1 in [(0,-3.8,2,5.1),(0,-.4,2,5.1),(1,5.1,-3.8,-.4)]:brickwall('Kiln masonry',axis,pos,u0,u1,.2,4.2,[])
geo('Kiln steep hipped roof',[(-4.05,1.85,4.15),(-.15,1.85,4.15),(-.15,5.35,4.15),(-4.05,5.35,4.15),(-2.65,3.05,7.55),(-1.55,3.05,7.55),(-1.55,4.15,7.55),(-2.65,4.15,7.55)],[(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],clay,'Kiln')
cube('Kiln cowl shadow',(-2.1,3.6,7.9),(1.02,1.02,.7),dark,'Kiln')
for x in [-2.68,-1.52]:
 for y in [3.02,4.18]:beam('Cowl post',(x,y,7.5),(x,y,8.45),.13,coll='Kiln')
for z in [7.7,7.9,8.1,8.3]:
 for y in [3.0,4.2]:cube('Cowl air louvre',(-2.1,y,z),(1.3,.14,.1),oak,'Kiln',.01)
 for x in [-2.7,-1.5]:cube('Cowl air louvre',(x,3.6,z),(.14,1.3,.1),oak,'Kiln',.01)
geo('Cowl pyramidal cap',[(-2.95,2.75,8.43),(-1.25,2.75,8.43),(-1.25,4.45,8.43),(-2.95,4.45,8.43),(-2.1,3.6,9.15)],[(0,1,4),(1,2,4),(2,3,4),(3,0,4),(3,2,1,0)],clay,'Kiln')
# Delivery apron and grain-handling props remain static dressing.
for x,y,z in [(4.75,-2.7,.48),(4.8,-3.5,.48),(4.72,-3.08,1.15)]:
 bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=10,location=(x,y,z)); o=bpy.context.object; o.name='Tied grain sack'; o.scale=(.3,.36,.47)
 for c in list(o.users_collection):c.objects.unlink(o)
 C['Workyard'].objects.link(o); o.data.materials.append(cloth)
 for p in o.data.polygons:p.use_smooth=True
 beam('Sack cord',(x-.09,y,z+.40),(x+.09,y,z+.40),.035,mat=oak,coll='Workyard')
for y in [-3.6,-3.25,-2.9,-2.55]:cube('Dispatch pallet',(4.7,y,.08),(1.15,.25,.12),oak,'Workyard',.015)
# Review rig is excluded from export.
cube('Neutral review ground',(0,0,-.09),(200,200,.15),ground,'Review')
world=bpy.data.worlds.new('Neutral daylight'); S.world=world; world.use_nodes=True; world.node_tree.nodes.get('Background').inputs[0].default_value=(.65,.72,.8,1); world.node_tree.nodes.get('Background').inputs[1].default_value=.65
bpy.ops.object.light_add(type='SUN',location=(6,-8,15)); sun=bpy.context.object; sun.name='Neutral sun'; sun.rotation_euler=(math.radians(24),math.radians(-28),math.radians(-30)); sun.data.energy=2.4; sun.data.angle=.15
for c in list(sun.users_collection):c.objects.unlink(sun)
C['Review'].objects.link(sun)
bpy.ops.object.light_add(type='AREA',location=(4,-8,12)); fill=bpy.context.object; fill.data.energy=900; fill.data.shape='DISK'; fill.data.size=10
fill.rotation_euler=(Vector((0,0,3))-fill.location).to_track_quat('-Z','Y').to_euler()
for c in list(fill.users_collection):c.objects.unlink(fill)
C['Review'].objects.link(fill)
bpy.ops.object.camera_add(location=(16,-18,13)); cam=bpy.context.object; cam.name='Camera_Hero'; cam.rotation_euler=(Vector((0,0,3.6))-cam.location).to_track_quat('-Z','Y').to_euler(); cam.data.type='ORTHO';cam.data.ortho_scale=16.8; S.camera=cam
for c in list(cam.users_collection):c.objects.unlink(cam)
C['Review'].objects.link(cam)
bpy.ops.wm.save_as_mainfile(filepath=str(J/'checkpoints'/('malthouse-r%d.blend'%REV)))
S.render.filepath=str(J/'renders'/('r%d-hero.png'%REV)); bpy.ops.render.render(write_still=True)
S.render.image_settings.file_format='JPEG'; S.render.image_settings.quality=92; S.render.filepath=str(J/'renders'/('r%d-hero.jpg'%REV)); bpy.data.images['Render Result'].save_render(S.render.filepath,scene=S)
print('MALTHOUSE_RENDER_DONE',REV)
