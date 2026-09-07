import bpy, math, random, pathlib, json, sys
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1]
REV=int(sys.argv[sys.argv.index('--')+1]) if '--' in sys.argv else 0
random.seed(7103)
bpy.ops.wm.read_factory_settings(use_empty=True)
S=bpy.context.scene; S.unit_settings.system='METRIC'; S.unit_settings.scale_length=1
collections={}
for name in ['Structure','Cladding','Roof','Sails','Access','Stonework','Hardware','Review']:
 c=bpy.data.collections.new(name);S.collection.children.link(c);collections[name]=c
def move(o,c):
 for x in list(o.users_collection):x.objects.unlink(o)
 collections[c].objects.link(o)
def material(name,source,rough,relief,color=(.08,.065,.045,1),metal=0):
 m=bpy.data.materials.new(name);m.use_nodes=True;n=m.node_tree.nodes;l=m.node_tree.links;b=n.get('Principled BSDF');b.inputs['Roughness'].default_value=rough;b.inputs['Metallic'].default_value=metal
 uv=n.new('ShaderNodeTexCoord')
 if source:
  t=n.new('ShaderNodeTexImage');t.name='ImageGen_native_color';t.image=bpy.data.images.load(str(P/'textures'/f'mill--{source}--worn--1254x1254--v1.png'),check_existing=True);l.new(uv.outputs['UV'],t.inputs['Vector']);l.new(t.outputs['Color'],b.inputs['Base Color'])
 else:b.inputs['Base Color'].default_value=color
 noise=n.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=95 if source=='stone' else 120;noise.inputs['Detail'].default_value=3
 mapping=n.new('ShaderNodeVectorMath');mapping.operation='MULTIPLY';mapping.inputs[1].default_value=(1,.06,1) if source in ['oak','roof'] else (1,1,1);l.new(uv.outputs['UV'],mapping.inputs[0]);l.new(mapping.outputs['Vector'],noise.inputs['Vector'])
 bump=n.new('ShaderNodeBump');bump.name='Independent_physical_relief';bump.inputs['Distance'].default_value=relief;bump.inputs['Strength'].default_value=.28;l.new(noise.outputs['Fac'],bump.inputs['Height']);l.new(bump.outputs['Normal'],b.inputs['Normal'])
 ramp=n.new('ShaderNodeMapRange');ramp.inputs['From Min'].default_value=0;ramp.inputs['From Max'].default_value=1;ramp.inputs['To Min'].default_value=rough-.08;ramp.inputs['To Max'].default_value=min(.98,rough+.08);l.new(noise.outputs['Fac'],ramp.inputs['Value']);l.new(ramp.outputs[0],b.inputs['Roughness'])
 return m
oak=material('Oak','oak',.82,.0012);roof=material('RoofTimber','roof',.88,.0018);stone=material('Limestone','stone',.9,.0015);iron=material('ForgedIron',None,.72,.0004,(.052,.037,.025,1),.62)
dark=oak.copy();dark.name='ShelteredOak';n=dark.node_tree.nodes;l=dark.node_tree.links;mix=n.new('ShaderNodeMixRGB');mix.blend_type='MULTIPLY';mix.inputs[0].default_value=1;mix.inputs[2].default_value=(.48,.45,.39,1);l.new(n.get('ImageGen_native_color').outputs['Color'],mix.inputs[1]);l.new(mix.outputs[0],n.get('Principled BSDF').inputs['Base Color'])
def uvmap(o,grain=2):
 uv=o.data.uv_layers.new(name='SurfaceMetres') if not o.data.uv_layers else o.data.uv_layers[0]
 off=(random.random(),random.random())
 for f in o.data.polygons:
  axis=max(range(3),key=lambda i:abs(f.normal[i])); axes=[i for i in range(3) if i!=axis];v=grain if grain in axes else axes[-1];u=next(i for i in axes if i!=v)
  for idx in f.loop_indices:
   co=o.data.vertices[o.data.loops[idx].vertex_index].co;uv.data[idx].uv=(co[u]+off[0],co[v]+off[1])
def box(name,loc,size,mat,col='Structure',bevel=.012,grain=2):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name=name;o.dimensions=size;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(mat);uvmap(o,grain);move(o,col)
 if bevel:
  mod=o.modifiers.new('Worn edge radius','BEVEL');mod.width=bevel;mod.segments=2
  mod=o.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL')
 return o
def beam(name,a,b,width,depth,mat=oak,col='Structure'):
 a=Vector(a);b=Vector(b);o=box(name,(a+b)/2,(width,depth,(b-a).length),mat,col,min(.008,width*.1));o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler();return o
def cylinder(name,loc,r,depth,mat,col='Hardware',axis='Y',vertices=24):
 bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=r,depth=depth,location=loc);o=bpy.context.object;o.name=name;o.data.materials.append(mat);uvmap(o);move(o,col)
 if axis=='Y':o.rotation_euler[0]=math.pi/2
 elif axis=='X':o.rotation_euler[1]=math.pi/2
 mod=o.modifiers.new('Soft rim','BEVEL');mod.width=.008;mod.segments=2
 return o
# Irregular rubble plinth with visible inset joints. Each rock is real geometry.
for row in range(5):
 n=22;rad=1.95-row*.045
 for i in range(n):
  a=2*math.pi*(i+.47*(row%2))/n;w=random.uniform(.44,.64);h=random.uniform(.20,.29)
  o=box(f'Rubble_{row}_{i}',((rad+(random.uniform(-.06,.06) if REV>=2 else 0))*math.cos(a),(rad+(random.uniform(-.06,.06) if REV>=2 else 0))*math.sin(a),.15+row*.25+(random.uniform(-.035,.035) if REV>=1 else 0)),(w,.62,h),stone,'Stonework',.023 if REV>=2 else .045 if REV>=1 else .06)
  for v in o.data.vertices:v.co+=Vector([random.uniform(-.08,.08) if REV>=2 else random.uniform(-.045,.045) for _ in range(3)])
  o.rotation_euler[2]=a+math.pi/2
  if REV>=3:
   loc=o.location.copy();bpy.data.objects.remove(o,do_unlink=True)
   bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2,radius=1,location=loc);o=bpy.context.object;o.name=f'Fieldstone_{row}_{i}';o.scale=(w*.64,.38,h*.65)
   bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
   for v in o.data.vertices:v.co+=Vector([random.uniform(-.019,.019) for _ in range(3)])
   o.rotation_euler[2]=a+math.pi/2;o.data.materials.append(stone);uvmap(o);move(o,'Stonework')
   for f in o.data.polygons:f.use_smooth=True
cylinder('Rubble_infill',(0,0,.58),1.94 if REV>=3 else 1.74,1.1,stone,'Stonework','Z',32)
box('Main_post',(0,0,2.25),(.65,.65,3.1),dark)
for a,b in [((-1.65,0,1.1),(1.65,0,1.1)),((0,-1.65,1.32),(0,1.65,1.32))]:beam('Trestle_crossbeam',a,b,.36,.4,dark)
for x,y in [(1.5,0),(-1.5,0),(0,1.5),(0,-1.5)]:beam('Quarter_bar',(x,y,1.4),(0,0,3.0),.26,.26,dark)
# Buck raised above the trestle, true plank shell with rear door/window voids.
bottom=1.65; eave=6.5;ridge=8.2; halfx=1.8;halfy=2.1
box('Main_floor',(0,0,bottom),(3.65,4.25,.18),dark)
for x in [-1.69,1.69]:
 for y in [-1.99,1.99]:box('Corner_post',(x,y,4.05),(.22,.22,4.8),oak)
for z in [1.8,4.1,6.35]:
 for y in [-1.99,1.99]:beam('Cross_girt',(-1.7,y,z),(1.7,y,z),.22,.22,dark)
 for x in [-1.69,1.69]:beam('Side_girt',(x,-2,z),(x,2,z),.22,.22,dark)
width=.185
for y in [-halfy,halfy]:
 for i in range(20):
  x=-1.8+(i+.5)*.18;top=eave+(ridge-eave)*(1-abs(x)/1.8)
  # Rear opening door and upper hatch, front inspection slit.
  intervals=[(bottom,top)]
  holes=[]
  if y>0 and -.54<x<.54:holes=[(1.8,3.6),(4.5,5.45)]
  if y<0 and -.36<x<.36:holes=[(2.75,3.35)]
  for lo,hi in holes:
   intervals=[q for a,b in intervals for q in ([(a,min(b,lo))] if a<lo else [])+([(max(a,hi),b)] if b>hi else []) if q[1]-q[0]>.01]
  for j,(a,b) in enumerate(intervals):
   o=box(f'End_plank_{y}_{i}_{j}',(x,y+random.uniform(-.007,.007),(a+b)/2),(.172,.065,b-a),oak,'Cladding',.005)
   if REV>=1 and b==top:
    for v in o.data.vertices:
     if v.co.z>0:v.co.z=eave+(ridge-eave)*(1-abs(x+v.co.x)/1.8)-o.location.z
for x in [-halfx,halfx]:
 for i in range(24):
  y=-2.1+(i+.5)*4.2/24;box('Side_plank',(x+random.uniform(-.005,.005),y,(bottom+eave)/2),(.065,.168,eave-bottom+random.uniform(-.025,.025)),oak,'Cladding',.005)
# Framed rear access and side shutters, no painted-on openings.
for lo,hi,w in [(1.8,3.6,1.08),(4.5,5.45,1.08)]:
 for x in [-w/2-.06,w/2+.06]:box('Opening_jamb',(x,2.15,(lo+hi)/2),(.11,.16,hi-lo+.18),dark,'Access')
 for z in [lo-.04,hi+.04]:box('Opening_head_sill',(0,2.18,z),(w+.23,.24,.11),oak,'Access',grain=0)
 for k in range(6):box('Open_shutter_board',(w/2+.11+k*.17,2.16,(lo+hi)/2),(.161,.055,hi-lo-.05),oak,'Access')
 for z in [lo+.18,hi-.18]:box('Shutter_iron_strap',(.99,2.204,z),(.84,.016,.035),iron,'Hardware',.003)
beam('Rear_tailpole',(0,2.2,5.5),(0,7.6,.55),.12,.15,oak,'Access')
beam('Tail_crossbrace',(0,2.2,1.7),(0,7.6,.55),.2,.2,dark,'Access')
for x in [-.66,.66]:
 beam('Stair_stringer',(x,2.5,1.73),(x,4.4,.18),.13,.17,oak,'Access')
 if REV>=2:
  beam('Stair_handrail',(x,2.45,2.65),(x,4.4,1.12),.055,.065,oak,'Access')
  for t in [0,.5,1]:
   y=2.45+1.95*t;z=1.73-1.55*t;beam('Stair_baluster',(x,y,z),(x,y,z+.9),.055,.055,oak,'Access')
for i in range(9):box('Stair_tread',(0,2.5+i*.23,1.72-i*.183),(1.48,.29,.075),oak,'Access',.01,0)
for i in range(8):box('Landing_board',(-.68+i*.19,2.36,1.78),(.18,.65,.07),oak,'Access',.007,1)
# Split shingles overlap down slope. Reference roof's modern sheet treatment intentionally omitted.
slope=math.atan2(1.7,1.98);length=math.hypot(1.98,1.7)
for sign in [-1,1]:
 if REV>=1:
  under=box('Roof_underboarding',(sign*.99,0,(eave+ridge)/2-.035),(length,4.6,.05),dark,'Roof',.003,0);under.rotation_euler[1]=sign*slope
 for row in range(12):
  dist=row*(length/12)
  for j in range(21):
   y=-2.37+(j+.5)*4.74/21+(row%2)*.045
   x=sign*(1.98-dist*math.cos(slope));z=eave+dist*math.sin(slope)+.05
   size=(.37,.218+random.uniform(-.012,.01),.024 if REV<1 else .035)
   o=box('Split_shingle',(x,y,z),size,roof,'Roof',.003,0);o.rotation_euler[1]=sign*slope+random.uniform(-.008,.008)
 for y in [-2.38,2.38]:beam('Gable_bargeboard',(0,y,ridge+.11),(sign*2.08,y,eave-.08),.13,.11,oak,'Roof')
beam('Ridge_cap',(0,-2.49,ridge+.15),(0,2.49,ridge+.15),.17,.18,roof,'Roof')
# Four lattice sails, hub/pivot aligned along Y, asymmetric leading board strip.
hub=Vector((0,-2.58,6.23));cylinder('Windshaft',hub,.23,1.35,dark,'Sails')
cylinder('Iron_hub_band',(0,-3.20,6.23),.265,.17,iron)
for aidx in range(4):
 angle=math.radians(35)+aidx*math.pi/2;d=Vector((math.sin(angle),0,math.cos(angle)));t=Vector((math.cos(angle),0,-math.sin(angle)))
 def pt(r,q=0):return hub+d*r+t*q
 beam('Sail_main_stock',pt(.18),pt(5.48),.16,.20,oak,'Sails')
 for q in [-.32,.25,.80]:beam('Sail_lattice_long',pt(1.13,q),pt(5.48,q),.045,.055,oak,'Sails')
 for j in range(17):
  r=1.15+j*4.33/16;beam('Sail_cross_lath',pt(r,-.38),pt(r,.86),.045,.042,oak,'Sails')
  if REV>=1:
   for q in [-.30,.27,.79]:cylinder('Sail_peg',pt(r,q)+Vector((0,-.035,0)),.013,.025,dark,'Hardware',vertices=8)
 for j in range(12):
  r=1.23+j*.34;beam('Leading_wind_board',pt(r,-.30),pt(r,.08),.33,.028,oak,'Sails')
 if REV>=3:
  for r in [.45,.77]:beam('Stock_iron_binding',pt(r,-.11)+Vector((0,-.105,0)),pt(r,.11)+Vector((0,-.105,0)),.09,.02,iron,'Hardware')
if REV>=2:
 # Recessed timber liner makes the small front opening legible, avoiding a floating cutout.
 for z in [2.73,3.37]:box('Front_hatch_lintel',(0,-2.13,z),(.85,.18,.095),dark,'Access',.008,0)
 for x in [-.405,.405]:box('Front_hatch_jamb',(x,-2.13,3.05),(.085,.16,.62),dark,'Access')
# Structural fixings and authentic small-scale wear geometry.
if REV>=2:
 for y in [-2.14,2.14]:
  for i in range(20):
   x=-1.8+(i+.5)*.18
   for z in [1.9,4.12,6.3]:
    if y>0 and abs(x)<.54 and z<3.6:continue
    cylinder('Cladding_peg',(x,y,z),.011,.014,dark,'Hardware',vertices=8)
if REV>=3:
 for side in [-1,1]:
  for i in range(9):
   y=-1.9+i*.45
   beam('Exposed_plank_end_check',(side*1.837,y,1.66),(side*1.837,y+.008,1.8+random.random()*.1),.004,.003,dark,'Hardware')
# Editable geometry-aware broad weathering. No photo luminance is converted to height.
if REV>=1:
 for c in collections.values():
  for o in c.objects:
   if o.type!='MESH':continue
   vc=o.data.vertex_colors.new(name='Weathering');tone=random.uniform(.70,1.02) if c.name=='Stonework' else random.uniform(.83,1.02)
   for f in o.data.polygons:
    for li in f.loop_indices:
     v=o.matrix_world@o.data.vertices[o.data.loops[li].vertex_index].co
     patch=.5+.5*math.sin(v.x*4.7+math.sin(v.y*6.1))*math.sin(v.y*3.8+.7)
     damp=max(0,1-v.z/1.25)*patch*.34 if c.name=='Stonework' else max(0,1-abs(v.z-1.67)/.35)*patch*.28
     t=tone*(1-damp);vc.data[li].color=(t,t*(1-.02*damp),t*(1-.11*damp),1)
 for mat in [oak,roof,stone,dark,iron]:
  n=mat.node_tree.nodes;l=mat.node_tree.links;b=n.get('Principled BSDF');incoming=list(b.inputs['Base Color'].links)
  vc=n.new('ShaderNodeVertexColor');vc.layer_name='Weathering';mix=n.new('ShaderNodeMixRGB');mix.name='Geometry_bound_weathering';mix.blend_type='MULTIPLY';mix.inputs[0].default_value=1
  if incoming:l.new(incoming[0].from_socket,mix.inputs[1])
  else:mix.inputs[1].default_value=b.inputs['Base Color'].default_value
  l.new(vc.outputs['Color'],mix.inputs[2]);l.new(mix.outputs[0],b.inputs['Base Color'])
# Neutral review stage, independent from exported mesh.
ground=material('ReviewGround',None,.95,0,(.19,.205,.18,1));box('Review_ground',(0,0,-.12),(200,200,.12),ground,'Review',0)
world=bpy.data.worlds.new('Neutral daylight');world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.67,.76,.88,1);world.node_tree.nodes['Background'].inputs[1].default_value=.7;S.world=world
def light(name,loc,energy,size):
 data=bpy.data.lights.new(name,'AREA');data.energy=energy;data.shape='DISK';data.size=size;o=bpy.data.objects.new(name,data);collections['Review'].objects.link(o);o.location=loc;o.rotation_euler=(Vector((0,0,4))-o.location).to_track_quat('-Z','Y').to_euler()
light('Large soft daylight',(-7,-9,16),1900,9);light('Sky fill',(7,3,12),950,8)
def camera(name,loc,target,lens):
 data=bpy.data.cameras.new(name);data.lens=lens;o=bpy.data.objects.new(name,data);collections['Review'].objects.link(o);o.location=loc;o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler();return o
camera('Hero',(15,-22,12),(0,0,5.5),48);camera('Rear',(-14,22,10),(0,1,5),48);camera('Timber_Detail',(6,-8,5),(1.1,-1.8,4.6),65);camera('Roof_Detail',(7,-9,10),(0,-.5,7.1),64);camera('Base_Detail',(5,7,3.5),(0,1.8,1.15),60);camera('Iron_Detail',(2,-5,7),(0,-2.7,6.23),65)
S.render.engine='BLENDER_EEVEE';S.eevee.use_gtao=True;S.eevee.gtao_distance=2;S.eevee.gtao_factor=1.08;S.eevee.taa_render_samples=64;S.eevee.use_soft_shadows=True
S.render.resolution_x=1200;S.render.resolution_y=1200;S.render.resolution_percentage=100;S.render.image_settings.file_format='PNG';S.view_settings.view_transform='Filmic';S.view_settings.look='Medium High Contrast';S.camera=bpy.data.objects['Hero']
bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints'/f'mill_r{REV}.blend'))
views=['Hero','Rear'] if REV==0 else ['Hero','Roof_Detail'] if REV==1 else ['Rear','Base_Detail'] if REV==2 else ['Hero','Rear','Timber_Detail','Roof_Detail','Base_Detail','Iron_Detail']
for name in views:
 S.camera=bpy.data.objects[name];S.render.filepath=str(P/'renders'/f'r{REV}_{name}.png');bpy.ops.render.render(write_still=True)
print('MILL_BUILD_COMPLETE',REV,flush=True)
