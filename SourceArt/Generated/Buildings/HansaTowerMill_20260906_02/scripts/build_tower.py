import bpy,math,random,pathlib,json,sys
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1];random.seed(3217)
REV=int(sys.argv[sys.argv.index('--')+1]) if '--' in sys.argv else 0
bpy.ops.wm.read_factory_settings(use_empty=True);S=bpy.context.scene;S.unit_settings.system='METRIC'
C={}
for name in ['Tower','Roof','Sails','Openings','Hardware','Review']:
 c=bpy.data.collections.new(name);S.collection.children.link(c);C[name]=c
def move(o,col):
 for c in list(o.users_collection):c.objects.unlink(o)
 C[col].objects.link(o)
def uvmap(o,grain=2,extent=1):
 uv=o.data.uv_layers.new(name='SurfaceMetres');off=(random.random(),random.random())
 for f in o.data.polygons:
  ax=max(range(3),key=lambda k:abs(f.normal[k]));axes=[k for k in range(3) if k!=ax];v=grain if grain in axes else axes[-1];u=next(k for k in axes if k!=v)
  for li in f.loop_indices:
   co=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(co[u]/extent+off[0],co[v]/extent+off[1])
def mat(name,source=None,color=(.1,.1,.1,1),rough=.8,relief=.001,metal=0):
 m=bpy.data.materials.new(name);m.use_nodes=True;n=m.node_tree.nodes;l=m.node_tree.links;b=n.get('Principled BSDF');b.inputs['Base Color'].default_value=color;b.inputs['Metallic'].default_value=metal
 uv=n.new('ShaderNodeTexCoord')
 if source:
  t=n.new('ShaderNodeTexImage');t.name='ImageGen_native_color';t.image=bpy.data.images.load(str(P/'textures'/f'tower--{source}--worn--1254x1254--v1.png'),check_existing=True);l.new(uv.outputs['UV'],t.inputs['Vector']);l.new(t.outputs[0],b.inputs['Base Color'])
 no=n.new('ShaderNodeTexNoise');no.inputs['Scale'].default_value=130;no.inputs['Detail'].default_value=3
 mp=n.new('ShaderNodeVectorMath');mp.operation='MULTIPLY';mp.inputs[1].default_value=(1,.055,1) if source in ['timber','paint'] else (1,1,1);l.new(uv.outputs['UV'],mp.inputs[0]);l.new(mp.outputs[0],no.inputs['Vector'])
 bump=n.new('ShaderNodeBump');bump.name='Independent_physical_relief';bump.inputs['Distance'].default_value=relief;bump.inputs['Strength'].default_value=.28;l.new(no.outputs['Fac'],bump.inputs['Height']);l.new(bump.outputs[0],b.inputs['Normal'])
 ramp=n.new('ShaderNodeMapRange');ramp.inputs['To Min'].default_value=rough-.08;ramp.inputs['To Max'].default_value=min(.98,rough+.08);l.new(no.outputs['Fac'],ramp.inputs['Value']);l.new(ramp.outputs[0],b.inputs['Roughness'])
 return m
masonry=mat('Masonry','masonry',rough=.91,relief=.003);wood=mat('WeatheredTimber','timber',rough=.84,relief=.0015);paint=mat('SagePaint','paint',rough=.8,relief=.0005)
brick=mat('OldBrick',color=(.26,.105,.058,1),rough=.89,relief=.0018);iron=mat('ForgedIron',color=(.045,.035,.029,1),rough=.76,relief=.0005,metal=.55);glass=mat('WindowGlass',color=(.035,.079,.084,1),rough=.21,relief=.00005);dark=mat('RecessTimber',color=(.046,.039,.032,1),rough=.9,relief=.001)
def mesh(name,verts,faces,material,col,extent=1):
 me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);me.update();o=bpy.data.objects.new(name,me);C[col].objects.link(o);me.materials.append(material);uvmap(o,extent=extent);return o
def box(name,loc,size,material,col='Hardware',bevel=.007,grain=2):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name=name;o.dimensions=size;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(material);uvmap(o,grain);move(o,col)
 if bevel:mod=o.modifiers.new('Worn edges','BEVEL');mod.width=bevel;mod.segments=2
 return o
def beam(name,a,b,w,d,material=wood,col='Sails'):
 a=Vector(a);b=Vector(b);o=box(name,(a+b)/2,(w,d,(b-a).length),material,col,min(.006,w*.09));o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler();return o
def cyl(name,loc,r,depth,material,col='Hardware',axis='Y',verts=24):
 bpy.ops.mesh.primitive_cylinder_add(vertices=verts,radius=r,depth=depth,location=loc);o=bpy.context.object;o.name=name;o.data.materials.append(material);uvmap(o);move(o,col)
 if axis=='Y':o.rotation_euler[0]=math.pi/2
 return o
def radius(z):return 4.0-1.45*z/8.7
# Hollow tapered stone shell, actual arched openings through thick wall.
bpy.ops.mesh.primitive_cone_add(vertices=128,radius1=4,radius2=2.55,depth=8.7,location=(0,0,4.35));tower=bpy.context.object;tower.name='Tapered_masonry_tower';move(tower,'Tower');tower.data.materials.append(masonry)
bpy.ops.mesh.primitive_cone_add(vertices=128,radius1=3.45,radius2=2.0,depth=8.8,location=(0,0,4.4));cut=bpy.context.object
def subtract(o,c):
 bpy.context.view_layer.objects.active=c;c.select_set(True);bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.normals_make_consistent(inside=False);bpy.ops.object.mode_set(mode='OBJECT');bpy.context.view_layer.objects.active=o;mod=o.modifiers.new('True masonry reveal','BOOLEAN');mod.operation='DIFFERENCE';mod.solver='EXACT';mod.object=c;bpy.ops.object.modifier_apply(modifier=mod.name);bpy.data.objects.remove(c,do_unlink=True)
subtract(tower,cut)
def archsolid(name,w,lo,shoulder,rise,y0,y1,material=dark,col='Openings'):
 profile=[(-w/2,lo),(w/2,lo),(w/2,shoulder)]+[(w/2*math.cos(a),shoulder+rise*math.sin(a)) for a in [math.pi*i/16 for i in range(1,17)]]
 n=len(profile);v=[(x,y,z) for y in [y0,y1] for x,z in profile];f=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
 return mesh(name,v,f,material,col)
openings=[(0,.05,1.85,.32,1.65),(0,3.35,4.08,.19,.72),(0,5.4,6.0,.15,.59),(0,7.2,7.65,.13,.43)]
for idx,(x,lo,sh,rise,w) in enumerate(openings):
 y=-radius((lo+sh)/2);cut=archsolid('Opening_cut',w,lo,sh,rise,y-1,y+1);subtract(tower,cut)
 # Brick arch voussoirs and jambs, with inset opening and sill.
 for j in range(13):
  a=math.pi*(j+.5)/13;xx=(w/2+.10)*math.cos(a);z=sh+(rise+.13)*math.sin(a);o=box('Brick_arch', (xx,-radius(z)-.035,z),(.14,.26,.24),brick,'Openings',.012);o.rotation_euler[1]=a-math.pi/2
 for side in [-1,1]:
  count=max(3,int((sh-lo)/.17))
  for j in range(count):
   z=lo+(j+.5)*(sh-lo)/count;box('Brick_jamb',(side*(w/2+.10),-radius(z)-.025,z),(.20,.22,(sh-lo)/count-.014),brick,'Openings',.012)
 if idx:
  y=-radius((lo+sh)/2)+.19
  archsolid('Inset_window',w*.94,lo+.025,sh-.025,rise*.9,y,y+.03,glass)
  for side in [-1,1]:box('Sage_window_jamb',(side*(w/2-.045),y-.028,(lo+sh)/2),(.065,.075,sh-lo+.05),paint,'Openings',.003)
  for z in [lo+.03,sh-.015]:box('Sage_window_rail',(0,y-.035,z),(w-.03,.08,.06),paint,'Openings',.003,0)
  box('Window_mullion',(0,y-.05,(lo+sh)/2),(.035,.05,sh-lo),paint,'Openings',.002)
  box('Window_transom',(0,y-.05,(lo+sh)/2),(w-.06,.05,.03),paint,'Openings',.002,0)
  box('Worn_stone_sill',(0,-radius(lo)-.065,lo-.06),(w+.32,.40,.12),masonry,'Openings',.019)
 else:
  for side in [-1,1]:
   for j in range(5):box('Open_sage_door_board',(side*(w/2+.10+j*.125),-radius(.9)-.12,.98),(.119,.055,1.85),paint,'Openings',.004)
   for z in [.38,1.6]:box('Door_iron_strap',(side*1.1,-radius(.9)-.16,z),(.52,.025,.055),iron,'Hardware',.003,0)
  box('Door_threshold',(0,-3.87,.055),(1.85,.65,.11),masonry,'Openings',.02)
  box('Interior_floor',(0,0,.035),(5.8,5.8,.07),dark,'Tower',0)
  cyl('Interior_post',(0,-1.1,1.6),.16,3.1,dark,'Tower','Z')
# Cylindrical UVs keep metric tile scale and put the only wrap seam at the back.
[tower.data.uv_layers.remove(u) for u in list(tower.data.uv_layers)];uv=tower.data.uv_layers.new(name='SurfaceMetres')
for f in tower.data.polygons:
 vals=[]
 for li in f.loop_indices:
  v=tower.matrix_world@tower.data.vertices[tower.data.loops[li].vertex_index].co;a=math.atan2(v.x,-v.y);vals.append((li,a,v.z))
 if max(a for _,a,_ in vals)-min(a for _,a,_ in vals)>math.pi:vals=[(li,a+2*math.pi if a<0 else a,z) for li,a,z in vals]
 for li,a,z in vals:uv.data[li].uv=(a*3.2/2.5,z/2.5)
for f in tower.data.polygons:f.use_smooth=abs(f.normal.z)<.5
# Mansard-like wooden cap: a broad shingled front, steep lower flanks and short upper roof.
profile=[(-2.82,8.76),(-1.58,12.15),(0,13.0),(1.58,12.15),(2.82,8.76)]
v=[(x,y,z) for y in [-2.15,2.15] for x,z in profile];n=5
cap=mesh('Cap_underboarding',v,[tuple(reversed(range(5))),tuple(range(5,10))]+[(i,i+1,i+6,i+5) for i in range(4)],dark,'Roof')
def halfwidth(z):return 2.82-(z-8.76)*(1.24/3.39) if z<=12.15 else 1.58*(13-z)/.85
for side in [-1,1]:
 for row in range(19):
  z=8.77+row*.222;w=max(.1,halfwidth(z));count=max(1,math.ceil(w*2/.24))
  for j in range(count):
   x=-w+(j+.5)*w*2/count
   if side==-1 and .42<abs(x)<.94 and 10.4<z<11.26:continue
   o=box('Cap_front_shingle',(x,side*(2.18+.014*(row%2)),z+.17),(2*w/count-.009,.034,.35),wood,'Roof',.003)
   # Fit each top to actual cap profile instead of leaving stepped gable gaps.
   for vert in o.data.vertices:
    gx=abs(x+vert.co.x);ztop=13-.85*gx/1.58 if gx<1.58 else 12.15-(gx-1.58)*3.39/1.24
    vert.co.z=min(vert.co.z,ztop-o.location.z)
 for k in range(2):
  a=Vector((side*(2.82 if k==0 else 1.58),0,8.76 if k==0 else 12.15));b=Vector((side*(1.58 if k==0 else 0),0,12.15 if k==0 else 13));length=(b-a).length;direction=(b-a).normalized()
  rows=math.ceil(length/.24)
  for row in range(rows):
   pos=a+direction*(row*length/rows+.12)
   for j in range(19):
    y=-2.31+(j+.5)*4.62/19;o=box('Roof_split_shingle',(pos.x,y,pos.z+.018),(.39,.235,.034),wood,'Roof',.003,0);o.rotation_euler[1]=-math.atan2(direction.z,direction.x)
  for y in [-2.25,2.25]:beam('Cap_bargeboard',(a.x,y,a.z),(b.x,y,b.z),.075,.085,wood,'Roof')
beam('Cap_ridge',(0,-2.32,13.04),(0,2.32,13.04),.12,.13,wood,'Roof')
for x in [-.68,.68]:
 box('Cap_glazing',(x,-2.175,10.87),(.48,.04,.85),glass,'Openings',0)
 for dx in [-.26,.26]:box('Cap_window_jamb',(x+dx,-2.24,10.86),(.055,.055,.94),paint,'Openings',.003)
 for z in [10.41,11.31]:box('Cap_window_rail',(x,-2.24,z),(.57,.055,.06),paint,'Openings',.003,0)
# Four long asymmetric lattice sails; all members have longitudinal grain.
hub=Vector((0,-2.64,10.92));cyl('Windshaft',hub,.24,1.25,wood,'Sails');cyl('Iron_hub',(0,-3.22,10.92),.32,.19,iron)
for k in range(4):
 angle=math.pi/4+k*math.pi/2;d=Vector((math.sin(angle),0,math.cos(angle)));t=Vector((math.cos(angle),0,-math.sin(angle)))
 def pt(r,q=0):return hub+d*r+t*q
 beam('Sail_stock',pt(-.2),pt(8.15),.19,.22)
 for q in [.20,.56,.92,1.28]:beam('Lattice_longitudinal',pt(1.4,q),pt(8.15,q),.046,.047)
 for j in range(25):
  r=1.43+j*6.69/24;beam('Lattice_cross_lath',pt(r,-.16),pt(r,1.32),.044,.042)
 for j in range(22):
  r=1.5+j*.30;beam('Leading_windboard',pt(r,-.24),pt(r,.14),.29,.027)
 for r in [.5,.95,1.4]:beam('Stock_binding',pt(r,-.12)+Vector((0,-.12,0)),pt(r,.15)+Vector((0,-.12,0)),.10,.02,iron,'Hardware')
# Per-member condition and location-aware dampness, retained as portable vertex colors.
for name,c in C.items():
 if name=='Review':continue
 for o in c.objects:
  if o.type!='MESH':continue
  vc=o.data.vertex_colors.new(name='Weathering');tone=random.uniform(.51,.78) if name=='Roof' else random.uniform(.62,.80) if name=='Sails' else random.uniform(.82,1.0)
  for f in o.data.polygons:
   for li in f.loop_indices:
    p=o.matrix_world@o.data.vertices[o.data.loops[li].vertex_index].co;damp=max(0,1-p.z/1.3)*(.14+.12*math.sin(p.x*2+p.y)**2);val=tone*(1-damp);vc.data[li].color=(val,val,val*.98,1)
for m in [masonry,wood,paint,brick,iron,glass,dark]:
 n=m.node_tree.nodes;l=m.node_tree.links;b=n.get('Principled BSDF');src=list(b.inputs['Base Color'].links);vc=n.new('ShaderNodeVertexColor');vc.layer_name='Weathering';mix=n.new('ShaderNodeMixRGB');mix.name='Geometry_bound_weathering';mix.blend_type='MULTIPLY';mix.inputs[0].default_value=1
 if src:l.new(src[0].from_socket,mix.inputs[1])
 else:mix.inputs[1].default_value=b.inputs['Base Color'].default_value
 l.new(vc.outputs[0],mix.inputs[2]);l.new(mix.outputs[0],b.inputs['Base Color'])
groundmat=mat('ReviewGround',color=(.28,.29,.27,1),rough=.95);box('Ground',(0,0,-.08),(200,200,.1),groundmat,'Review',0)
world=bpy.data.worlds.new('Overcast daylight');world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.72,.78,.85,1);world.node_tree.nodes['Background'].inputs[1].default_value=.8;S.world=world
for name,loc,power,size in [('Daylight',(-10,-14,22),2800,12),('Fill',(8,3,16),1300,10)]:
 data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size;o=bpy.data.objects.new(name,data);C['Review'].objects.link(o);o.location=loc;o.rotation_euler=(Vector((0,0,6))-o.location).to_track_quat('-Z','Y').to_euler()
def camera(name,loc,target,lens=52):
 d=bpy.data.cameras.new(name);d.lens=lens;o=bpy.data.objects.new(name,d);C['Review'].objects.link(o);o.location=loc;o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler();return o
camera('Front',(0,-34,8.5),(0,0,8),51);camera('Hero',(18,-32,15),(0,0,8),48);camera('Rear',(-20,32,13),(0,0,7),48);camera('Base_Detail',(3,-11,4),(0,-3,2.4),58);camera('Roof_Detail',(8,-13,14),(0,-1,11),54);camera('Iron_Detail',(2,-9,12),(0,-2.5,10.9),55)
S.render.engine='BLENDER_EEVEE';S.eevee.use_gtao=True;S.eevee.gtao_distance=1;S.eevee.gtao_factor=1;S.eevee.taa_render_samples=48;S.eevee.use_soft_shadows=True
S.render.resolution_x=1000;S.render.resolution_y=1200;S.render.resolution_percentage=100;S.render.image_settings.file_format='PNG';S.view_settings.view_transform='Filmic';S.view_settings.look='Medium High Contrast';S.camera=bpy.data.objects['Front']
bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints'/f'tower_r{REV}.blend'))
for name in ['Front','Hero']:
 S.camera=bpy.data.objects[name];S.render.filepath=str(P/'renders'/f'r{REV}_{name}.png');bpy.ops.render.render(write_still=True)
print('TOWER_BUILD_COMPLETE',flush=True)
