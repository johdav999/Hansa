import bpy, math, random, json
from mathutils import Vector
from pathlib import Path
J=Path(__file__).resolve().parents[1];random.seed(23)
bpy.ops.wm.open_mainfile(filepath=str(J/'checkpoints/malthouse-r2.blend'))
oak=bpy.data.materials['M_MaltHouse_Oak'];iron=bpy.data.materials['M_MaltHouse_Iron'];cloth=bpy.data.materials['M_MaltHouse_Sacking'];dark=bpy.data.materials['M_MaltHouse_Recess']
def assign(o,n,mat,coll='Workyard'):
 o.name=n
 for c in list(o.users_collection):c.objects.unlink(o)
 bpy.data.collections[coll].objects.link(o);o.data.materials.append(mat);o.data.use_auto_smooth=True
 return o
def box(n,p,d,m,coll='Workyard',b=.01):
 bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=assign(bpy.context.object,n,m,coll);o.dimensions=d;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 for poly in o.data.polygons:
  a=max(range(3),key=lambda k:abs(poly.normal[k]));axes=[k for k in range(3) if k!=a];axes.sort(key=lambda k:d[k])
  for li in poly.loop_indices:
   co=o.data.vertices[o.data.loops[li].vertex_index].co;o.data.uv_layers.active.data[li].uv=(co[axes[0]]/2.5,co[axes[1]]/2.5)
 if b:
  mod=o.modifiers.new('Handworked edges','BEVEL');mod.width=b;mod.segments=2;o.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL')
 return o
def beam(n,a,b,w=.1,m=oak):
 a,b=Vector(a),Vector(b);o=box(n,(a+b)/2,(w,w,(b-a).length),m);o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler();return o
def cylinder(n,p,r,d,m,rotation=(0,0,0)):
 bpy.ops.mesh.primitive_cylinder_add(vertices=24,radius=r,depth=d,location=p,rotation=rotation);return assign(bpy.context.object,n,m)
for o in list(bpy.data.collections['Workyard'].objects):
 if o.name.startswith(('Tied grain sack','Sack cord')):bpy.data.objects.remove(o,do_unlink=True)
for k,(x,y,z) in enumerate([(4.75,-2.7,.16),(4.75,-3.48,.16),(4.70,-3.09,.83)]):
 verts=[];faces=[];rings=[(0,.19),(.07,.32),(.2,.345),(.43,.34),(.60,.29),(.72,.17),(.77,.075),(.81,.07),(.90,.12)]
 for iz,(h,r) in enumerate(rings):
  for i in range(24):
   t=i*math.tau/24;rr=r*(1+.045*math.sin(5*t+iz*.45)+.028*math.sin(9*t));verts.append((x+rr*math.cos(t),y+rr*1.12*math.sin(t),z+h))
 for iz in range(len(rings)-1):
  for i in range(24):faces.append((iz*24+i,iz*24+(i+1)%24,(iz+1)*24+(i+1)%24,(iz+1)*24+i))
 faces.extend([tuple(reversed(range(24))),tuple(range(192,216))])
 mesh=bpy.data.meshes.new('Folded sack');mesh.from_pydata(verts,[],faces);mesh.update();o=bpy.data.objects.new('Folded tied grain sack',mesh);bpy.data.collections['Workyard'].objects.link(o);mesh.materials.append(cloth);uv=mesh.uv_layers.new()
 for p in mesh.polygons:
  p.use_smooth=True
  for li in p.loop_indices:
   vi=mesh.loops[li].vertex_index;uv.data[li].uv=((vi%24)/24,(vi//24)/8)
 bpy.ops.mesh.primitive_torus_add(major_radius=.076,minor_radius=.012,major_segments=24,minor_segments=6,location=(x,y,z+.795));assign(bpy.context.object,'Tied hemp sack cord',oak)
 beam('Loose cord end',(x+.07,y,z+.79),(x+.13,y,z+.63),.015,oak)
# Woven cloth relief is separately authored, not a conversion of color brightness.
nodes=cloth.node_tree.nodes;links=cloth.node_tree.links;p=nodes.get('Principled BSDF');tex=nodes.new('ShaderNodeTexCoord');wx=nodes.new('ShaderNodeTexWave');wy=nodes.new('ShaderNodeTexWave');wx.bands_direction='X';wy.bands_direction='Y'
for w in [wx,wy]:w.inputs['Scale'].default_value=135;links.new(tex.outputs['UV'],w.inputs['Vector'])
mix=nodes.new('ShaderNodeMath');mix.operation='MULTIPLY';links.new(wx.outputs['Color'],mix.inputs[0]);links.new(wy.outputs['Color'],mix.inputs[1]);bump=nodes.new('ShaderNodeBump');bump.inputs['Distance'].default_value=.001;bump.inputs['Strength'].default_value=.22;links.new(mix.outputs[0],bump.inputs['Height']);links.new(bump.outputs['Normal'],p.inputs['Normal'])
# Real hoisting hardware and angled brace under the cantilever.
beam('Hoist timber brace',(4.06,-1,5.05),(5.10,-1,5.75),.13)
cylinder('Grain hoist sheave',(5.16,-1,5.62),.16,.12,oak,(math.pi/2,0,0))
cylinder('Sheave iron axle',(5.16,-1,5.62),.038,.22,iron,(math.pi/2,0,0))
beam('Hoist rope',(5.27,-1,5.6),(5.27,-1,3.25),.025,oak)
bpy.ops.mesh.primitive_torus_add(major_radius=.075,minor_radius=.015,major_segments=16,minor_segments=6,location=(5.27,-1,3.18),rotation=(math.pi/2,0,0));assign(bpy.context.object,'Hoist hook eye',iron)
# Kiln fire-door and soot stay localized to the fire intake, on the accessible courtyard face.
box('Kiln fire opening',(-.28,3.72,.78),(.06,1.0,1.12),dark,'Kiln',.025)
for y in [3.12,4.32]:box('Kiln fired surround',(-.20,y,.8),(.25,.22,1.5),bpy.data.materials['M_MaltHouse_Brick'],'Kiln')
box('Kiln lintel',(-.2,3.72,1.55),(.28,1.42,.24),bpy.data.materials['M_MaltHouse_Brick'],'Kiln')
for i in range(7):box('Kiln iron fire grille',(-.15,3.32+i*.135,.85),(.04,.033,.85),iron,'Kiln',.004)
box('Kiln hearth slab',(.18,3.72,.12),(.9,1.5,.24),bpy.data.materials['M_MaltHouse_Mortar'],'Kiln',.025)
# Low work table and wide shallow soaking tub make the courtyard useful.
box('Malt sorting table',(2.0,3.25,1.02),(1.6,.85,.12),oak)
for x in [1.35,2.65]:
 for y in [2.95,3.55]:box('Table leg',(x,y,.53),(.12,.12,.95),oak)
for i in range(6):box('Malt tray boards',(2.0,2.88+i*.145,1.16),(1.4,.135,.10),oak)
for y in [2.80,3.70]:box('Malt tray rim',(2,y,1.25),(1.55,.07,.22),oak)
for x in [1.23,2.77]:box('Malt tray rim',(x,3.25,1.25),(.07,.92,.22),oak)
beam('Malt shovel shaft',(2.6,3.8,.15),(3.4,3.8,1.6),.055);o=box('Malt shovel blade',(2.65,3.8,.24),(.34,.07,.46),oak);o.rotation_euler.y=-.4
for i in range(20):
 t=i*math.tau/20;x=3.4+.61*math.cos(t);y=4.55+.61*math.sin(t);o=box('Steeping tub stave',(x,y,.5),(.19,.055,.84),oak);o.rotation_euler.z=t+math.pi/2
for z in [.23,.73]:
 bpy.ops.mesh.primitive_torus_add(major_radius=.63,minor_radius=.019,major_segments=32,minor_segments=6,location=(3.4,4.55,z));assign(bpy.context.object,'Steeping tub hoop',iron)
cylinder('Steeping tub dark interior',(3.4,4.55,.16),.6,.08,dark)
# Slight roof-edge weathering is expressed in geometry; surface color remains free of lighting.
for y in [-4.31,2.31]:box('Timber eave fascia',(0,y,3.49),(8.6,.10,.16),oak,'Timber')
S=bpy.context.scene
def render(n,loc=None,target=(0,0,3.6),scale=16.8):
 cam=S.camera
 if loc:cam.location=loc;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=scale
 S.render.image_settings.file_format='PNG';S.render.filepath=str(J/'renders'/('r3-'+n+'.png'));bpy.ops.render.render(write_still=True);S.render.image_settings.file_format='JPEG';S.render.image_settings.quality=92;bpy.data.images['Render Result'].save_render(str(J/'renders'/('r3-'+n+'.jpg')),scene=S)
render('hero');render('courtyard',(15,18,12));render('rear',(-16,18,12));render('roof-detail',(8,-9,10),(0,-1.5,5.6),6.2);render('door-detail',(12,-8,5),(4,-1.5,2.2),4.7)
S.camera.location=(16,-18,13);S.camera.rotation_euler=(Vector((0,0,3.6))-S.camera.location).to_track_quat('-Z','Y').to_euler();S.camera.data.ortho_scale=16.8;S.render.image_settings.file_format='PNG'
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(J/'checkpoints/malthouse-r3.blend'))
