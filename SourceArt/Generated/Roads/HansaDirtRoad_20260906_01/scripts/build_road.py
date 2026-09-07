import bpy, math, sys, json, pathlib, random
from mathutils import Vector
from math import sin,cos,pi,sqrt,exp
P=pathlib.Path(__file__).resolve().parents[1]
ROOT=P.parents[2]
REV=int(sys.argv[sys.argv.index('--')+1]) if '--' in sys.argv else 1
for d in ['renders','exports','checkpoints','textures','references','evidence']: (P/d).mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
S=bpy.context.scene; S.unit_settings.system='METRIC'; S.unit_settings.scale_length=1
random.seed(704)
source=next((P/'textures').glob('*basecolor*.png'))
base=bpy.data.images.load(str(source));base.colorspace_settings.name='sRGB'
def node(m,typ):return m.node_tree.nodes.new(typ)
def wire(m,a,out,b,inp):m.node_tree.links.new(a.outputs[out],b.inputs[inp])
def maps():
 if (P/'textures/Dirt_Normal.png').exists():return
 bpy.ops.mesh.primitive_plane_add(size=2)
 plane=bpy.context.object
 m=bpy.data.materials.new('Editable_Grain_Bake');m.use_nodes=True
 plane.data.materials.append(m);bs=m.node_tree.nodes.get('Principled BSDF')
 uv=node(m,'ShaderNodeTexCoord');noise=node(m,'ShaderNodeTexNoise');noise.inputs['Scale'].default_value=210;noise.inputs['Detail'].default_value=2
 # A 4D torus gives exactly periodic procedural grain independent of pigment.
 sep=node(m,'ShaderNodeSeparateXYZ');wire(m,uv,'UV',sep,'Vector')
 combine=node(m,'ShaderNodeCombineXYZ')
 for axis in ['X','Y']:
  mul=node(m,'ShaderNodeMath');mul.operation='MULTIPLY';mul.inputs[1].default_value=2*pi;wire(m,sep,axis,mul,0)
  trig=node(m,'ShaderNodeMath');trig.operation='SINE';wire(m,mul,0,trig,0);wire(m,trig,0,combine,axis)
 wire(m,combine,0,noise,'Vector')
 bump=node(m,'ShaderNodeBump');bump.inputs['Distance'].default_value=.0014;bump.inputs['Strength'].default_value=.35;wire(m,noise,'Fac',bump,'Height');wire(m,bump,'Normal',bs,'Normal')
 rough=node(m,'ShaderNodeMapRange');rough.inputs['To Min'].default_value=.77;rough.inputs['To Max'].default_value=.95;wire(m,noise,'Fac',rough,'Value');wire(m,rough,'Result',bs,'Roughness')
 S.render.engine='CYCLES';S.cycles.samples=4;S.render.bake.margin=0
 for name,kind in [('Dirt_Normal','NORMAL'),('Dirt_Roughness','ROUGHNESS')]:
  im=bpy.data.images.new(name,width=1024,height=1024,alpha=False);im.colorspace_settings.name='Non-Color'
  target=node(m,'ShaderNodeTexImage');target.image=im;m.node_tree.nodes.active=target
  bpy.ops.object.bake(type=kind);im.filepath_raw=str(P/'textures'/f'{name}.png');im.file_format='PNG';im.save()
 bpy.data.objects.remove(plane,do_unlink=True)
maps()
m=bpy.data.materials.new('M_DirtRoad');m.use_nodes=True;bs=m.node_tree.nodes.get('Principled BSDF');bs.inputs['Specular'].default_value=.22
tex=node(m,'ShaderNodeTexImage');tex.image=base
col=node(m,'ShaderNodeVertexColor');col.layer_name='RoadColor'
mix=node(m,'ShaderNodeMixRGB');mix.blend_type='MULTIPLY';mix.inputs[0].default_value=1
wire(m,tex,'Color',mix,1);wire(m,col,'Color',mix,2);wire(m,mix,'Color',bs,'Base Color')
for name,slot in [('Dirt_Normal','Normal'),('Dirt_Roughness','Roughness')]:
 im=bpy.data.images.load(str(P/'textures'/f'{name}.png'));im.colorspace_settings.name='Non-Color';n=node(m,'ShaderNodeTexImage');n.image=im
 if slot=='Normal':
  nm=node(m,'ShaderNodeNormalMap');nm.inputs['Strength'].default_value=.5;wire(m,n,'Color',nm,'Color');wire(m,nm,'Normal',bs,slot)
 else:wire(m,n,'Color',bs,slot)
def clamp(x):return min(1,max(0,x))
def smooth(x):x=clamp(x);return x*x*(3-2*x)
def smin(a,b,k=.65):h=max(k-abs(a-b),0)/k;return min(a,b)-h*h*k*.25
def profile(t,s,fade=1,rutfade=1):
 shoulder=smooth((abs(t)-1.65)/.75)
 wear=exp(-((abs(t)-(.82 + (.055*fade*sin(s*1.1+(0 if t>0 else 1.6)) if REV>=4 else 0)))/(.23 if REV>=2 else .16))**2)*rutfade
 crown=(.045 if REV>=2 else .09)*max(0,1-(abs(t)/2.4)**2)
 z=crown-(.025 if REV>=2 else .055)*wear-.075*shoulder
 z+=fade*(.006*sin(s*2.5+t*5)+.003*sin(s*9-t*11))
 color=(.90 + (.035 if REV>=4 else .08)*wear)*(1-(.24 if REV>=3 else .43)*shoulder)
 color*=1+fade*.025*sin(s*1.3+t*2)
 return z,(color,color*.98,color*.93,1)
meshes=[];stats={}
def make(name,verts,faces,uvs,colors):
 mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
 ob=bpy.data.objects.new(name,mesh);S.collection.objects.link(ob);mesh.materials.append(m)
 uv=mesh.uv_layers.new(name='UV0_Tile_2m');vc=mesh.vertex_colors.new(name='RoadColor');uv=mesh.uv_layers[0]
 for face in mesh.polygons:
  face.use_smooth=True
  for li in face.loop_indices:
   vi=mesh.loops[li].vertex_index;uv.data[li].uv=uvs[vi];vc.data[li].color=colors[vi]
 ob['role']='draft modular dirt road';ob['metres_per_uv_tile']=2.;ob['forward_axis']='X';ob['width_m']=4.8
 meshes.append(ob);return ob
def strip(name,corner=False,end=False):
 verts=[];faces=[];uvs=[];colors=[];nx=80 if not corner else 96;ny=48
 for i in range(nx+1):
  f=i/nx;s=8*f if not corner else 6*pi/2*f
  fade=sin(pi*f)**2
  for j in range(ny+1):
   t=-2.4+j*.1;edge=smooth((abs(t)-1.6)/.8)
   t+=edge*fade*(.045*sin(s*3.3)+.025*sin(s*8.1))*(1 if t>0 else -1)
   z,c=profile(t,s,fade)
   if corner:
    a=-pi/2+pi/2*f;x=(6-t)*cos(a);y=(6-t)*sin(a)
   else:x=s-4;y=t
   if end:
    x=s*.625-2.5
    taper=smooth((f-.30)/.70)
    y*=1-.42*taper
    z-=.10*taper
    c=tuple(v*(1-.2*taper) for v in c[:3])+(1,)
   verts.append((x,y,z));uvs.append((x/2,y/2));colors.append(c)
 for i in range(nx):
  for j in range(ny):
   a=i*(ny+1)+j;b=a+ny+1
   faces.append((a,b,b+1,a+1))
 return make(name,verts,faces,uvs,colors)
straight=strip('SM_DirtRoad_Straight_8m')
corner=strip('SM_DirtRoad_Corner90_R6m',True)
end=strip('SM_DirtRoad_End_5m',end=True)
def junction(name,T=False):
 verts=[];faces=[];uvs=[];colors=[];lookup={}
 def dist(x,y):return smin(abs(y),abs(x) if not T else math.hypot(x,min(y,0)))
 def sdf(p):x,y=p;return dist(x,y)-2.4
 def add(p):
  x,y=p;key=(round(x,7),round(y,7))
  if key in lookup:return lookup[key]
  d=dist(x,y);s=x if abs(x)>abs(y) else y
  fade=smooth((6-max(abs(x),abs(y)))/1.2)
  z,c=profile(d,s,fade,smooth((max(abs(x),abs(y))-1.4)/2.4))
  verts.append((x,y,z));uvs.append((x/2,y/2));colors.append(c);lookup[key]=len(verts)-1;return len(verts)-1
 def clip(poly):
  out=[]
  for a,b in zip(poly,poly[1:]+poly[:1]):
   da,db=sdf(a),sdf(b)
   if da<=1e-8:out.append(a)
   if (da<0 and db>0) or (da>0 and db<0):
    f=da/(da-db);out.append((a[0]+f*(b[0]-a[0]),a[1]+f*(b[1]-a[1])))
  return out
 for i in range(120):
  for j in range(120):
   x=-6+i*.1;y=-6+j*.1
   poly=clip([(x,y),(x+.1,y),(x+.1,y+.1),(x,y+.1)])
   if len(poly)>=3:
    inds=list(dict.fromkeys(add(p) for p in poly))
    if len(inds)>=3:faces.append(inds)
 return make(name,verts,faces,uvs,colors)
tee=junction('SM_DirtRoad_TJunction_12m',True)
cross=junction('SM_DirtRoad_Crossroads_12m')
for ob in meshes:
 bpy.context.view_layer.objects.active=ob;ob.select_set(True)
 mesh=ob.data;print('VALIDATING',ob.name,len(mesh.vertices),len(mesh.polygons),flush=True);assert not mesh.validate(verbose=True);mesh.update();mesh.calc_loop_triangles();assert len(mesh.loop_triangles)>0
 assert all(p.area>1e-11 for p in mesh.polygons)
 assert all(p.normal.z>.4 for p in mesh.polygons),ob.name
 stats[ob.name]={'vertices':len(mesh.vertices),'triangles':len(mesh.loop_triangles),'bounds_m':[list(min(v.co[k] for v in mesh.vertices) for k in range(3)),list(max(v.co[k] for v in mesh.vertices) for k in range(3))],'uv_channels':len(mesh.uv_layers),'material_slots':1}
 ob.select_set(False)
# Exact source seam checks (road junction connectors share the same profile).
rows=[]
for j in range(49):
 t=-2.4+j*.1;z,c=profile(t,0,0)
 rows.append({'y_m':t,'z_m':z,'color':c})
(P/'evidence'/f'geometry_v{REV}.json').write_text(json.dumps(stats,indent=2))
(P/'evidence'/'connector_profile.json').write_text(json.dumps(rows,indent=2))
# Exports use asset-local origins, identity transforms, +X length and +Z up.
if REV>=4:
 for ob in meshes:
  bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob
  tri=ob.modifiers.new('Export_Triangulation','TRIANGULATE');tri.keep_custom_normals=True
  bpy.ops.export_scene.fbx(filepath=str(P/'exports'/(ob.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,mesh_smooth_type='FACE',use_tspace=True,path_mode='COPY',embed_textures=True)
  bpy.ops.export_scene.gltf(filepath=str(P/'exports'/(ob.name+'.glb')),use_selection=True,export_format='GLB',export_yup=True,export_colors=True,export_tangents=True,export_apply=True)
  ob.modifiers.remove(tri)
# Review scene: laid out kit and a bent copy made from the actual straight topology.
positions={straight:(-9,-10,0),corner:(7,-5,0),end:(-9,0,0),tee:(-9,12,0),cross:(8,12,0)}
for ob,pos in positions.items():ob.location=pos
demo=straight.copy();demo.data=straight.data.copy();demo.name='QA_SplineDeformation';S.collection.objects.link(demo);demo.location=(0,-12,0)
for v in demo.data.vertices:
 x,y,z=v.co;s=x+4;a=s/10-.4
 v.co=(10*sin(a)-y*sin(a),10*(1-cos(a))+y*cos(a),z+.45*sin(pi*s/8)**2)
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.15));ground=bpy.context.object;ground.name='QA_Ground'
gm=bpy.data.materials.new('QA_NeutralGround');gm.diffuse_color=(.10,.12,.10,1);gm.use_nodes=True;gbs=gm.node_tree.nodes.get('Principled BSDF');gbs.inputs['Base Color'].default_value=(.10,.12,.10,1);gbs.inputs['Roughness'].default_value=.95;ground.data.materials.append(gm)
world=bpy.data.worlds.new('Neutral_Daylight');S.world=world;world.use_nodes=True;world.node_tree.nodes.get('Background').inputs[0].default_value=(.65,.74,.85,1);world.node_tree.nodes.get('Background').inputs[1].default_value=.7
def sun(name,rot,energy):
 d=bpy.data.lights.new(name,'SUN');d.energy=energy;d.angle=.15;o=bpy.data.objects.new(name,d);S.collection.objects.link(o);o.rotation_euler=rot;return o
light=sun('QA_Daylight',(math.radians(32),math.radians(-25),math.radians(-32)),2.5)
def camera(name,loc,target,lens=48):
 d=bpy.data.cameras.new(name);o=bpy.data.objects.new(name,d);S.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler();d.lens=lens;return o
cameras=[camera('Kit',(34,-46,48),(0,2,0),45),camera('Surface',(-7,-14,3),(-9,-9,0),48),camera('Junction',(-18,3,13),(-9,12,0),45),camera('Spline',(9,-22,8),(0,-11,0),46)]
S.render.engine='BLENDER_EEVEE';S.eevee.use_gtao=True;S.eevee.gtao_distance=.3;S.eevee.gtao_factor=1;S.eevee.taa_render_samples=48
S.view_settings.view_transform='Filmic';S.view_settings.look='Medium High Contrast';S.view_settings.exposure=0;S.view_settings.gamma=1
S.render.resolution_x=1100;S.render.resolution_y=760;S.render.resolution_percentage=100;S.render.image_settings.file_format='JPEG';S.render.image_settings.quality=92
for im in bpy.data.images:
 if im.source=='FILE':im.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints'/f'DirtRoad_v{REV}.blend'))
for cam in cameras:
 S.camera=cam;S.render.filepath=str(P/'renders'/f'v{REV}_{cam.name}.jpg');bpy.ops.render.render(write_still=True)
if REV>=4:
 S.camera=cameras[0];bpy.ops.wm.save_as_mainfile(filepath=str(P/'exports'/'Hansa_DirtRoad_Kit.blend'))
print('ROAD_BUILD_DONE',REV,stats)




