import bpy, bmesh, math, random, os, sys, json
from mathutils import Vector
from pathlib import Path
random.seed(1701)
JOB=Path(__file__).resolve().parents[1]
REV=int(os.environ.get('BAKERY_REV','0'))
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
for m in list(bpy.data.materials): bpy.data.materials.remove(m)
S=bpy.context.scene; S.unit_settings.system='METRIC'; S.unit_settings.scale_length=1
COL={}
for name in ['Structure','Masonry','Roof','Openings','Timber','Hardware','Bakery','Review']:
 c=bpy.data.collections.new(name); S.collection.children.link(c); COL[name]=c

def move(o,c):
 for cc in list(o.users_collection): cc.objects.unlink(o)
 COL[c].objects.link(o); return o

def mat(name,color,rough=.75,scale=30,bump=.012,metal=0,wood=False):
 m=bpy.data.materials.new(name); m.use_nodes=True; m.diffuse_color=(*color,1)
 n=m.node_tree.nodes; l=m.node_tree.links; p=n.get('Principled BSDF'); p.inputs['Roughness'].default_value=rough; p.inputs['Metallic'].default_value=metal
 tex=n.new('ShaderNodeTexCoord'); mp=n.new('ShaderNodeVectorMath'); mp.operation='MULTIPLY'; mp.inputs[1].default_value=(2,65,3) if wood else (1,1,1); l.new(tex.outputs['Object'],mp.inputs[0])
 noise=n.new('ShaderNodeTexNoise'); noise.inputs['Scale'].default_value=scale; noise.inputs['Detail'].default_value=3; l.new(mp.outputs[0],noise.inputs['Vector'])
 ramp=n.new('ShaderNodeValToRGB'); ramp.color_ramp.elements[0].position=.12; ramp.color_ramp.elements[1].position=.85
 ramp.color_ramp.elements[0].color=(*(v*.76 for v in color),1); ramp.color_ramp.elements[1].color=(*(min(1,v*1.13) for v in color),1)
 l.new(noise.outputs['Fac'],ramp.inputs[0]); l.new(ramp.outputs[0],p.inputs['Base Color'])
 fine=n.new('ShaderNodeTexNoise'); fine.inputs['Scale'].default_value=170; fine.inputs['Detail'].default_value=2; l.new(mp.outputs[0],fine.inputs['Vector'])
 bn=n.new('ShaderNodeBump'); bn.inputs['Strength'].default_value=.3; bn.inputs['Distance'].default_value=bump; l.new(fine.outputs['Fac'],bn.inputs['Height']); l.new(bn.outputs[0],p.inputs['Normal'])
 m['physical_tile_extent_m']=2.; m['source']='Original procedural'; return m
brick=[mat('Brick_'+str(i),c,.86,24,.008) for i,c in enumerate([(.29,.12,.065),(.36,.16,.085),(.235,.105,.062)])]
clay=[mat('Clay_'+str(i),c,.79,45,.006) for i,c in enumerate([(.31,.115,.051),(.37,.15,.065),(.255,.09,.038)])]
lime=mat('Lime_Plaster',(.59,.55,.445),.86,5,.0025)
stone=mat('Limestone',(.36,.33,.26),.84,48,.01)
mortar=mat('Lime_Mortar',(.35,.32,.26),.91,90,.004)
oak=mat('Weathered_Oak',(.105,.069,.038),.76,4,.008,wood=True)
iron=mat('Forged_Iron',(.039,.045,.044),.58,30,.003,.78)
lead=mat('Lead_Roofing',(.12,.14,.15),.58,28,.002,.7)
glass=mat('Window_Glass',(.038,.055,.057),.22,70,.0005)
glass.node_tree.nodes.get('Principled BSDF').inputs['Transmission'].default_value=.55
bread=mat('Rye_Crust',(.39,.205,.078),.91,32,.007)
linen=mat('Flour_Linen',(.58,.52,.39),.95,100,.004)
soot=mat('Oven_Soot',(.036,.029,.023),.98,42,.003)

def box(name,loc,dim,material,c='Structure',bevel=.018):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc); o=bpy.context.object; o.name=name; o.dimensions=dim; bpy.ops.object.transform_apply(location=False,rotation=False,scale=True); move(o,c); o.data.materials.append(material)
 if bevel:
  mod=o.modifiers.new('Worn_Edges','BEVEL'); mod.width=bevel; mod.segments=2
  o.modifiers.new('Weighted_Normals','WEIGHTED_NORMAL')
 return o

def mesh(name,verts,faces,material,c='Structure'):
 me=bpy.data.meshes.new(name); me.from_pydata(verts,[],faces); me.update(); bm=bmesh.new(); bm.from_mesh(me); bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces)); bm.to_mesh(me); bm.free(); o=bpy.data.objects.new(name,me); COL[c].objects.link(o); o.data.materials.append(material); return o

def beam(name,a,b,w,d,material=oak,c='Timber'):
 a,b=Vector(a),Vector(b); o=box(name,(a+b)/2,(w,d,(b-a).length),material,c,.008); o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler(); return o

def tube(name,points,r,material,c='Hardware',cyclic=False):
 cv=bpy.data.curves.new(name,'CURVE'); cv.dimensions='3D'; cv.resolution_u=2; cv.bevel_depth=r; cv.bevel_resolution=3
 sp=cv.splines.new('POLY'); sp.points.add(len(points)-1)
 for p,co in zip(sp.points,points):p.co=(*co,1)
 sp.use_cyclic_u=cyclic; o=bpy.data.objects.new(name,cv); COL[c].objects.link(o); o.data.materials.append(material); return o

# Rounded, pointed lancet polygon: front is -Y, real physical depth.
def archpoly(x,z,w,h):
 spring=z+h-w*.63; pts=[(x-w/2,z),(x+w/2,z),(x+w/2,spring)]
 for k in range(1,9):
  t=k/8; pts.append((x+w/2*(1-t),spring+w*.63*(math.sin(t*math.pi/2))))
 for k in range(1,9):
  t=k/8; pts.append((x-w/2*t,spring+w*.63*math.cos(t*math.pi/2)))
 return pts

def extrude(name,poly,y,depth,material,c):
 N=len(poly); v=[(x,yy,z) for yy in [y,y+depth] for x,z in poly]; f=[tuple(reversed(range(N))),tuple(range(N,2*N))]+[(i,(i+1)%N,(i+1)%N+N,i+N) for i in range(N)]
 return mesh(name,v,f,material,c)

def opening(x,z,w,h,y=-6.53,arched=True,shutter=False):
 poly=archpoly(x,z,w,h) if arched else [(x-w/2,z),(x+w/2,z),(x+w/2,z+h),(x-w/2,z+h)]
 o=extrude('Deep_reveal',poly,y,.23,stone,'Openings')
 p2=[(x+(xx-x)*.83,z+.09+(zz-z)*.9) for xx,zz in poly]
 extrude('Dark_glazed_opening',p2,y-.006,.015,oak if shutter else glass,'Openings')
 tube('Cut_stone_arch',[(xx,y-.055,zz) for xx,zz in poly[2:]],.068,stone,'Openings')
 box('Stone_sill',(x,y-.12,z-.02),(w+.2,.38,.14),stone,'Openings',.025)
 if shutter:
  for j in range(4):box('Shutter_plank',(x-w*.35+j*w*.23,y-.036,z+h*.42),(w*.21,.04,h*.73),oak,'Timber',.009)
  for zz in [z+.22,z+h*.66]:box('Shutter_iron_strap',(x,y-.078,zz),(w*.84,.025,.045),iron,'Hardware',.007)
 else:
  box('Oak_mullion',(x,y-.057,z+h*.47),(.055,.06,h*.85),oak,'Openings',.004)
  for zz in [z+h*.34,z+h*.64]:box('Window_crossbar',(x,y-.052,zz),(w*.84,.07,.047),oak,'Openings',.004)
  # modest lead lattice
  for dz in [.25,.5,.75]:tube('Leaded_pane',[(x-w*.4,y-.055,z+h*dz),(x+w*.4,y-.055,z+h*dz)],.009,iron,'Openings')

# Main envelope with cutouts through actual walls.
wall=box('Lime_ground_and_dwelling',(0,0,3.2),(9,13,5.95),lime,bevel=.025)
cutouts=[(-2.9,.48,1.8,2.05),(0,.12,1.5,2.75),(2.9,.65,1.8,1.7),(-2.9,3.65,1.6,1.75),(0,3.65,1.6,1.75),(2.9,3.65,1.6,1.75)]
for x,z,w,h in cutouts:
 cutter=box('Cutter',(x,-6.4,z+h/2),(w,.9,h),lime,bevel=0)
 mod=wall.modifiers.new('Recess','BOOLEAN'); mod.operation='DIFFERENCE'; mod.object=cutter; bpy.context.view_layer.objects.active=wall; bpy.ops.object.modifier_apply(modifier=mod.name); bpy.data.objects.remove(cutter,do_unlink=True)
# source envelope hollow interior for shop window depth
cutter=box('Interior_void',(0,0,3.1),(8.35,12.3,5.9),lime,bevel=0)
mod=wall.modifiers.new('Hollow_shell','BOOLEAN'); mod.object=cutter; mod.operation='DIFFERENCE'; bpy.context.view_layer.objects.active=wall; bpy.ops.object.modifier_apply(modifier=mod.name); bpy.data.objects.remove(cutter,do_unlink=True)
box('Shop_floor',(0,0,.18),(9,13,.2),stone)
box('Dwelling_floor',(0,0,3.18),(8.8,12.8,.22),oak)
for x,z,w,h in cutouts[3:]:opening(x,z,w,h,-6.33,False)
# foundation stones
for side in [-1,1]:
 for k in range(19):box('Foundation_stone',(side*4.46,-6.25+k*.69,.27),(.4,.66,.5),stone,'Masonry',.035)
for k in range(18):
 x=-4.25+k*.5
 if abs(x)>.85:box('Foundation_front',(x,-6.48,.25),(.48,.38,.46),stone,'Masonry',.03)
# Upper gable contour follows observed concave wings and tall paired piers.
def top(x):
 a=abs(x)
 if a<1.8:return 15.2
 t=(a-1.8)/2.7
 return 14.8-4.7*math.sqrt(max(0,t))
openings=[]
for z,xs,h in [(6.45,[-3,-1,1,3],1.24),(8.42,[-3,-1,1,3],1.65),(10.9,[-1,1],1.45),(12.85,[-1,1],1.48)]:
 for x in xs:
  for dx in [-.24,.24]:openings.append((x+dx,z,.36,h))
poly=[(-4.5,6.17),(4.5,6.17)]+[(4.5-i*9/80,top(4.5-i*9/80)) for i in range(81)]
front=extrude('Brick_gable_mortar',poly,-6.49,.34,mortar,'Structure')
for x,z,w,h in openings:
 cutter=extrude('Lancet_cutter',archpoly(x,z,w+.04,h+.03),-6.8,1,stone,'Openings')
 mod=front.modifiers.new('Lancet_void','BOOLEAN'); mod.object=cutter; mod.operation='DIFFERENCE'; bpy.context.view_layer.objects.active=front; bpy.ops.object.modifier_apply(modifier=mod.name); bpy.data.objects.remove(cutter,do_unlink=True)
 opening(x,z,w,h,-6.42,True,shutter=z>10)
# Efficient individually bevelled bricks combined by material.
buckets=[([],[]) for _ in brick]
def brickcube(loc,dim,idx):
 v,f=buckets[idx]; x,y,z=loc; a,b,c=[q/2 for q in dim]; off=len(v)
 v.extend([(x+dx*a,y+dy*b,z+dz*c) for dx,dy,dz in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]])
 f.extend([tuple(off+i for i in face) for face in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]])
for row in range(82):
 z=6.22+row*.112
 for col in range(35):
  x=-4.47+col*.265+(row%2)*.1325
  if x>4.47 or z+.06>top(x):continue
  if any(abs(x-xx)<w/2+.16 and zz-.06<z<zz+h+.07 for xx,zz,w,h in openings):continue
  brickcube((x,-6.515,z),(.25,.13,.098),random.choices(range(3),[5,3,2])[0])
# side and rear masonry above plaster
for side in [-1,1]:
 box('Upper_side_substrate',(side*4.36,0,7.85),(.27,13,3.4),mortar)
 for row in range(30):
  for j in range(48):brickcube((side*4.5,-6.4+j*.271+(row%2)*.13,6.23+row*.112),(.14,.256,.1),random.randrange(3))
# rear plain gable and upper wall
extrude('Rear_gable',[(-4.5,6.17),(4.5,6.17),(4.5,9.58),(0,14.32),(-4.5,9.58)],6.2,.3,brick[0],'Structure')
for i,(v,f) in enumerate(buckets):
 o=mesh('Handmade_brick_courses_'+str(i),v,f,brick[i],'Masonry'); b=o.modifiers.new('Soft_brick_edges','BEVEL'); b.width=.006;b.segments=2;o.modifiers.new('Brick_normals','WEIGHTED_NORMAL')
# narrow piers with layered reveals and octagonal caps
for x,h in [(-4.4,10.6),(-1.82,15.4),(0,16.05),(1.82,15.4),(4.4,10.6)]:
 start=6.2
 box('Gothic_pier',(x,-6.62,(start+h)/2),(.27,.4,h-start),brick[0],'Masonry',.025)
 for dx in [-.19,.19]:box('Pier_moulding',(x+dx,-6.55,(start+h-.12)/2),(.085,.17,h-start-.12),brick[1],'Masonry',.012)
 for z in [6.28+i*.225 for i in range(int((h-6.28)/.225))]:box('Pier_mortar_joint',(x,-6.827,z),(.26,.009,.014),mortar,'Masonry',0)
 bpy.ops.mesh.primitive_cone_add(vertices=8,radius1=.3,radius2=.28,depth=.16,location=(x,-6.59,h));o=move(bpy.context.object,'Masonry');o.name='Pier_cap';o.data.materials.append(stone)
 bpy.ops.mesh.primitive_cone_add(vertices=8,radius1=.31,radius2=.025,depth=.55,location=(x,-6.59,h+.34));o=move(bpy.context.object,'Roof');o.name='Lead_pinnacle';o.data.materials.append(lead)
# coping follows concave silhouette
for side in [-1,1]:
 pts=[(side*(1.85+i*2.55/35),-6.49,top(side*(1.85+i*2.55/35))+.03) for i in range(36)]
 tube('Gable_coping',pts,.073,clay[2],'Roof')
# main roof substrate and tile geometry
pitch=math.atan2(4.78,4.65); length=math.sqrt(4.78**2+4.65**2)
for side in [-1,1]:
 o=box('Roof_deck',(side*2.325,.35,11.93),(length,12.5,.12),oak,'Roof');o.rotation_euler[1]=side*pitch
 if REV==0:
  o=box('Tile_roof_blockout',(side*2.325,.35,12.02),(length,12.5,.10),clay[0],'Roof');o.rotation_euler[1]=side*pitch
 else:
  # Each tile is a gently arched clay shell with a real overlapping lip.
  vv=[];ff=[];mi=[]
  rows=30; columns=45
  for r in range(rows):
   dist=.05+r*(length/rows)
   for col in range(columns):
    yy=-5.98+col*.278+(r%2)*.035
    off=len(vv); tilelen=.30; width=.274; rise=.034
    for layer in [0,.022]:
     for u in [0,1]:
      for k in range(7):
       q=k/6; along=dist+u*tilelen; xx=side*(.025+along*math.cos(pitch)); zz=14.40-along*math.sin(pitch)+rise*math.sin(math.pi*q)+layer+(rows-r)*.002
       vv.append((xx,yy+q*width,zz))
    for k in range(6):ff.append(tuple(off+t for t in [14+k,15+k,22+k,21+k]));mi.append(random.randrange(3) if k==0 else mi[-1])
    ff.append(tuple(off+t for t in [7,13,27,21]));mi.append(mi[-1])
    ff.append(tuple(off+t for t in [0,14,20,6]));mi.append(mi[-1])
    ff.append(tuple(off+t for t in [0,7,21,14]));mi.append(mi[-1])
    ff.append(tuple(off+t for t in [6,20,27,13]));mi.append(mi[-1])
  o=mesh('Overlapping_pan_tiles_'+str(side),vv,ff,clay[0],'Roof');o.data.materials.append(clay[1]);o.data.materials.append(clay[2])
  for p,i in zip(o.data.polygons,mi):p.material_index=i;p.use_smooth=True
for j in range(49):
 y=-6.0+j*.257; tube('Ridge_cap',[(.19*math.cos(k*math.pi/10),y,14.4+.19*math.sin(k*math.pi/10)) for k in range(11)],.034,clay[1],'Roof')
# Ground shop fitout: central plank door and actual open display bay
for x,z,w,h in cutouts[:3]:
 for xx in [x-w/2-.06,x+w/2+.06]:box('Shop_jamb',(xx,-6.55,z+h/2),(.13,.23,h+.15),oak,'Timber')
 box('Shop_lintel',(x,-6.56,z+h+.07),(w+.32,.24,.18),oak,'Timber')
 box('Shop_threshold',(x,-6.68,z),(w+.24,.56,.16),stone,'Openings')
 if x==0:
  for j in range(8):box('Entry_door_plank',(-.66+j*.187,-6.36,1.48),(.179,.08,2.62),oak,'Timber',.009)
  for z0 in [.48,2.28]:box('Door_strap',(0,-6.42,z0),(1.36,.042,.08),iron,'Hardware')
  tube('Door_pull',[(.43+.062*math.cos(t*math.tau/24),-6.48,1.45+.078*math.sin(t*math.tau/24)) for t in range(24)],.012,iron,cyclic=True)
 else:
  box('Shop_dark_interior',(x,-5.87,1.6),(w,.08,h),oak,'Bakery',.01)
  box('Sales_counter',(x,-6.76,.96),(w+.18,.75,.12),oak,'Bakery')
  for sgn in [-1,1]:
   sh=box('Open_shop_shutter',(x+sgn*(w/2+.48),-6.69,1.75),(.8,.07,1.55),oak,'Timber');sh.rotation_euler[2]=sgn*.28
   for zz in [1.18,2.18]:box('Shutter_strap',(x+sgn*(w/2+.48),-6.76,zz),(.66,.05,.045),iron,'Hardware')
  for r in range(2):
   for j in range(5):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=8,radius=1,location=(x-.67+j*.33,-6.88+r*.28,1.13));o=move(bpy.context.object,'Bakery');o.name='Rye_loaf';o.scale=(.145,.22,.105);o.data.materials.append(bread)
# Eyebrow over shop, supported oak corbels
for x in [-3,3]:
 roof=box('Shop_weather_canopy',(x,-7.00,2.88),(2.7,1.25,.12),oak,'Timber');roof.rotation_euler[0]=-.20
 for dx in [-1,1]:beam('Canopy_bracket',(x+dx,-6.55,2.3),(x+dx,-7.35,2.85),.085,.085)
# projecting bread guild sign without text
beam('Sign_bracket',(3.8,-6.5,4.6),(3.8,-7.85,4.6),.045,.045,iron,'Hardware')
box('Bread_trade_sign',(3.8,-7.65,4.12),(.08,.67,.67),oak,'Bakery')
for y in [-7.88,-7.41]:tube('Sign_chain',[(3.8,y,4.43),(3.8,y,4.61)],.012,iron)
bpy.ops.mesh.primitive_uv_sphere_add(segments=24,ring_count=12,location=(3.745,-7.65,4.12));o=move(bpy.context.object,'Bakery');o.name='Carved_bread_emblem';o.scale=(.06,.24,.14);o.data.materials.append(bread)
# bakehouse rear extension with masonry oven and a correctly routed chimney
box('Bakehouse',(2.0,8.45,1.95),(4.9,4.0,3.6),lime)
roof=box('Bakehouse_lean_roof',(2.0,8.55,4.05),(5.3,4.5,.16),clay[0],'Roof');roof.rotation_euler[0]=-.16
box('Oven_chimney',(2.9,7.5,7.1),(.86,.86,7.5),brick[0],'Masonry',.025)
for z in [9.9,10.4,10.8]:box('Chimney_crown',(2.9,7.5,z),(1.04,1.04,.16),brick[1],'Masonry')
box('Chimney_flue',(2.9,7.5,10.84),(.66,.66,.03),soot,'Masonry')
for j in range(13):box('Chimney_course',(2.9,7.05,8.6+j*.17),(.85,.014,.015),mortar,'Masonry',0)
# rear oven arch (inferred external working mouth), firewood and handling props
extrude('Oven_arch',archpoly(2, .6,1.55,1.85),10.49,.14,brick[1],'Bakery')
extrude('Oven_mouth',archpoly(2,.7,1.05,1.24),10.65,.02,soot,'Bakery')
box('Oven_hearth',(2,10.7,.68),(1.85,.9,.17),stone,'Bakery')
for j in range(10):
 a=Vector((.2+(j%3)*.24,10.75,.33+(j//3)*.19)); beam('Firewood',a,a+Vector((0,.75,0)),.18,.17,oak,'Bakery')
# utility yard barrels, sacks, baker peel
for x,y in [(-3.9,-7.0),(4.9,4.8)]:
 bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=12,location=(x,y,.54));o=move(bpy.context.object,'Bakery');o.name='Flour_sack';o.scale=(.28,.26,.52);o.data.materials.append(linen)
beam('Bread_peel_handle',(4.85,9.8,.4),(4.85,9.55,2.7),.036,.045,oak,'Bakery')
box('Bread_peel_blade',(4.85,9.55,2.77),(.38,.04,.51),oak,'Bakery',.07)
# structural roof fascia and timber doors on rear
for side in [-1,1]:beam('Eaves_fascia',(side*4.68,-6.4,9.52),(side*4.68,6.65,9.52),.14,.15)
for x in [-2,1]:opening(x,7,1,1.6,6.54,False,True)
# Additional physical details in later inspected revisions
if REV>=2:
 for x in [-3,3]:
  for j in range(10):box('Canopy_clay_tile',(x-1.23+j*.275,-7.01,2.99),(.263,1.21,.048),clay[j%3],'Roof',.012)
 for side in [-1,1]:
  for y in [-5,-2,1,4]:
   beam('Eave_rafter',(side*4.38,y,9.2),(side*4.8,y,9.65),.11,.11)
 for x in [-3,-1,1,3]:
  for z in [7.8,10.14]:box('Sill_drip_course',(x,-6.61,z),(1.15,.27,.085),stone,'Masonry',.015)
 # wood plank separation on front shutters
 for x in [-3.8,-2.0,2.0,3.8]:
  for j in range(4):box('Shutter_board',(x-.29+j*.18,-6.78,1.72),(.17,.022,1.48),oak,'Timber',.006)
if REV>=3:
 # Ground splash marks are restricted to the lowest plaster; no uniform all-over dirt.
 damp=mat('Damp_Lime',(.39,.375,.3),.94,12,.004)
 for x in [-4.2,-1.72,1.83,4.25]:
  box('Localized_plaster_repair',(x,-6.505,.68),(.22,.012,.39),damp,'Structure',.025)
 # Further stone quoins and hinge fixings at close inspection.
 for side in [-1,1]:
  for k in range(11):box('Corner_quoin',(side*4.48,-6.43,.52+k*.47),(.28,.35,.435),stone,'Masonry',.026)
 for x in [-.58,.58]:
  for zz in [.48,2.28]:
   bpy.ops.mesh.primitive_uv_sphere_add(segments=8,ring_count=4,radius=.026,location=(x,-6.46,zz));o=move(bpy.context.object,'Hardware');o.name='Forged_rivet';o.data.materials.append(iron)
# studio ground not part of asset
box('Review_ground',(0,0,-.14),(200,200,.2),mat('Review_Ground',(.22,.235,.22),.9,2,.001),'Review',0)
world=bpy.data.worlds.new('Neutral_daylight') if not bpy.data.worlds else bpy.data.worlds[0];S.world=world;world.use_nodes=True;world.node_tree.nodes.get('Background').inputs[0].default_value=(.65,.74,.85,1);world.node_tree.nodes.get('Background').inputs[1].default_value=.6
bpy.ops.object.light_add(type='SUN',location=(0,0,20));sun=move(bpy.context.object,'Review');sun.name='Daylight_Sun';sun.rotation_euler=(math.radians(28),math.radians(-24),math.radians(-30));sun.data.energy=3;sun.data.angle=.12
bpy.ops.object.light_add(type='AREA',location=(1,-14,17));a=move(bpy.context.object,'Review');a.data.energy=1800;a.data.size=12;a.rotation_euler=(Vector((0,0,7))-a.location).to_track_quat('-Z','Y').to_euler()
def camera(name,loc,target,lens=45):
 bpy.ops.object.camera_add(location=loc);c=move(bpy.context.object,'Review');c.name=name;c.rotation_euler=(Vector(target)-c.location).to_track_quat('-Z','Y').to_euler();c.data.lens=lens;c.data.clip_end=300;return c
cam=camera('Hero',(24,-33,21),(0,0,7.4),48);S.camera=cam
camera('Facade',(0,-32,10),(0,-6,8),48)
camera('Rear',(24,31,20),(0,2,7),48)
camera('Detail_Shop',(9,-17,7),(2,-6.5,2.3),58)
camera('Detail_Gable',(10,-20,15),(0,-6.5,11),58)
camera('Detail_Roof',(16,9,19),(2,0,12),55)
S.render.engine='BLENDER_EEVEE';S.eevee.use_gtao=True;S.eevee.gtao_distance=3;S.eevee.gtao_factor=1.1;S.eevee.taa_render_samples=64;S.eevee.use_soft_shadows=True;S.render.resolution_x=1200;S.render.resolution_y=1200;S.render.resolution_percentage=100
S.view_settings.view_transform='Filmic';S.view_settings.look='Medium High Contrast';S.view_settings.exposure=0;S.view_settings.gamma=1
S.render.image_settings.file_format='PNG';S.render.filepath=str(JOB/'renders'/f'r{REV}_hero.png')
S['asset_role']='Hansa bakery exterior; reconstructed adaptation of Muehlenstrasse 1';S['period']='Early modern c.1650 adaptation; inferred rear and shop';S['closest_intended_view_m']=5
bpy.ops.wm.save_as_mainfile(filepath=str(JOB/'checkpoints'/f'bakery_r{REV}.blend'))
bpy.ops.render.render(write_still=True)
print('BAKERY_RENDER_COMPLETE',REV)


