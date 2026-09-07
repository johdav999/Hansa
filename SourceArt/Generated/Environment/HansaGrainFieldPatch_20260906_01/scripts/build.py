import bpy, math, random, json, sys
from pathlib import Path
from mathutils import Vector
P=Path(__file__).resolve().parents[1]
REV=int(sys.argv[sys.argv.index('--')+1]) if '--' in sys.argv else 0
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
S=bpy.context.scene; S.unit_settings.system='METRIC'; S.unit_settings.scale_length=1
S.render.engine='CYCLES'; S.cycles.samples=24; S.cycles.use_denoising=True
S.render.resolution_x=1200; S.render.resolution_y=1000; S.render.resolution_percentage=100
S.render.image_settings.file_format='JPEG'; S.render.image_settings.quality=90
S.view_settings.view_transform='Filmic'; S.view_settings.look='Medium High Contrast'; S.view_settings.exposure=0
S.world.color=(.35,.35,.35)
def material(name,image,tint,rough):
 m=bpy.data.materials.new(name); m.use_nodes=True; n=m.node_tree.nodes; l=m.node_tree.links; bs=n.get('Principled BSDF'); bs.inputs['Roughness'].default_value=rough
 tx=n.new('ShaderNodeTexImage'); tx.image=bpy.data.images.load(str(P/'textures'/image),check_existing=True)
 mix=n.new('ShaderNodeMixRGB'); mix.blend_type='MULTIPLY'; mix.inputs[0].default_value=1; mix.inputs[2].default_value=(*tint,1); l.new(tx.outputs['Color'],mix.inputs[1]); l.new(mix.outputs[0],bs.inputs['Base Color'])
 if REV>=3 and 'Soil' not in name:
  sat=n.new('ShaderNodeHueSaturation');sat.inputs['Saturation'].default_value=.57;l.new(mix.outputs[0],sat.inputs['Color']);l.new(sat.outputs[0],bs.inputs['Base Color'])
 # Independent physical grain, not luminance-derived relief.
 noise=n.new('ShaderNodeTexNoise'); noise.inputs['Scale'].default_value=180; noise.inputs['Detail'].default_value=2
 bump=n.new('ShaderNodeBump'); bump.inputs['Strength'].default_value=.12; bump.inputs['Distance'].default_value=.00015
 l.new(noise.outputs['Fac'],bump.inputs['Height']); l.new(bump.outputs['Normal'],bs.inputs['Normal'])
 return m
grainfile='grain-straw--mature--1254x1254--v1.png'
mats=[material('M_Grain_Straw',grainfile,(.77,.70,.49),.72),material('M_Grain_Husk',grainfile,(1,.94,.77),.79),material('M_Grain_Leaf',grainfile,(.67,.64,.39),.81)]
soil=material('M_Grain_Soil','field-soil--dry--1254x1254--v1.png',(1,1,1),.94)
verts=[]; faces=[]; uvs=[]; mids=[]
def face(vs,uv,mi):
 k=len(verts); verts.extend(vs); faces.append(tuple(range(k,k+len(vs)))); uvs.extend(uv); mids.append(mi)
def tube(a,b,r,mi=0,sides=4,tip=None):
 a=Vector(a); b=Vector(b); z=(b-a).normalized(); x=z.cross(Vector((0,1,0))).normalized(); y=z.cross(x); rt=r if tip is None else tip
 for j in range(sides):
  t=2*math.pi*j/sides; t2=2*math.pi*(j+1)/sides
  p=x*math.cos(t)+y*math.sin(t); q=x*math.cos(t2)+y*math.sin(t2)
  face([a+p*r,a+q*r,b+q*rt,b+p*rt],[(j/sides,0),((j+1)/sides,0),((j+1)/sides,(b-a).length/.1),(j/sides,(b-a).length/.1)],mi)
def kernel(c,z,side,w,ln):
 c=Vector(c); z=Vector(z).normalized(); x=Vector(side).normalized(); y=z.cross(x).normalized()
 lo=c-z*ln*.5; hi=c+z*ln*.5+x*w*.35
 count=6 if REV>=3 else 4
 ring=[c+x*w*math.cos(math.tau*j/count)+y*w*.68*math.sin(math.tau*j/count) for j in range(count)]
 for j in range(count):
  k=(j+1)%count
  face([lo,ring[k],ring[j]],[(.5,0),(k/count,.5),(j/count,.5)],1)
  face([hi,ring[j],ring[k]],[(.5,1),(j/count,.5),(k/count,.5)],1)
def makeplant(x,y,rng):
 h=rng.uniform(.83,1.12) if REV==0 else rng.uniform(.78,1.20)
 angle=rng.random()*math.tau; dr=Vector((math.cos(angle),math.sin(angle),0)); side=Vector((-dr.y,dr.x,0))
 lean=rng.uniform(.015,.09) if REV<2 else rng.uniform(.015,.15)
 base=Vector((x,y,0)); pts=[base+Vector((0,0,h*t))+dr*lean*t*t for t in [0,.34,.70,1]]
 for a,b in zip(pts,pts[1:]):tube(a,b,.0035 if REV==0 else .0028)
 for j in range(3):
  t=.25+j*.21; a=base+Vector((0,0,h*t))+dr*lean*t*t
  d=dr*((-1)**j); length=rng.uniform(.17,.29); width=.018 if REV<3 else .012
  b=a+d*length*.52+Vector((0,0,.09)); c=a+d*length+Vector((0,0,.015 if j==2 else -.09)); s=side*width*.5
  face([a,b-s,b+s],[(.5,0),(0,.5),(1,.5)],2); face([b-s,c,b+s],[(0,.5),(.5,1),(1,.5)],2)
  # back faces retain leaves in portable opaque exports
  face([b+s,b-s,a],[(1,.5),(0,.5),(.5,0)],2); face([b+s,c,b-s],[(1,.5),(.5,1),(0,.5)],2)
 earlen=rng.uniform(.105,.15); axis=(Vector((0,0,1))+dr*(.12 if REV<2 else rng.uniform(.15,.45))).normalized()
 tube(pts[-1],pts[-1]+axis*earlen,.002,1)
 for j in range(14):
  t=(j+.5)/14; sg=(-1)**j; c=pts[-1]+axis*earlen*t+side*sg*.006
  w=.008*(.55+.45*math.sin(math.pi*t)); kernel(c,axis+side*sg*.55,side,w,.025)
  if REV>=1 and j%2==0:
   a=c+axis*.01+side*sg*w; b=a+axis*rng.uniform(.025,.045)+side*sg*.011; tube(a,b,.00055,1,3,0)
 return h+earlen
N=32 if REV==0 else 40
rng=random.Random(8316); heights=[]
for j in range(N):
 for i in range(N):
  x=-2+(i+.5+rng.uniform(-.4,.4))*4/N; y=-2+(j+.5+rng.uniform(-.4,.4))*4/N
  heights.append(makeplant(x,y,rng))
mesh=bpy.data.meshes.new('GrainPatch_Geometry'); mesh.from_pydata(verts,[],faces); mesh.update()
ob=bpy.data.objects.new('SM_GrainFieldPatch_4m',mesh); S.collection.objects.link(ob)
for m in mats:mesh.materials.append(m)
uv=mesh.uv_layers.new(name='UVMap')
for poly,mi in zip(mesh.polygons,mids):
 poly.material_index=mi
 for li in poly.loop_indices:uv.data[li].uv=uvs[mesh.loops[li].vertex_index]
# Geometric wind weight available to a future UE vertex-offset material.
col=mesh.vertex_colors.new(name='WindWeight')
for poly in mesh.polygons:
 for li in poly.loop_indices:
  z=mesh.vertices[mesh.loops[li].vertex_index].co.z; col.data[li].color=(min(1,max(0,z/1.35)),0,0,1)
if REV>=3:
 bpy.context.view_layer.objects.active=ob;ob.select_set(True)
 bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.remove_doubles(threshold=.000001);bpy.ops.mesh.normals_make_consistent(inside=False);bpy.ops.object.mode_set(mode='OBJECT')
 for poly in mesh.polygons:poly.use_smooth=poly.material_index!=2
bpy.ops.mesh.primitive_plane_add(size=4); ground=bpy.context.object; ground.name='SM_GrainFieldSoil_4m'; ground.data.materials.append(soil)
for loop in ground.data.uv_layers.active.data:loop.uv*=4
ground.location.z=-.008
# Display ground is separate; no perimeter rim or duplicate vertical faces.
def aim(obj,at):obj.rotation_euler=(Vector(at)-obj.location).to_track_quat('-Z','Y').to_euler()
def camera(name,loc,at,ortho):
 bpy.ops.object.camera_add(location=loc); c=bpy.context.object; c.name=name; c.data.type='ORTHO'; c.data.ortho_scale=ortho; aim(c,at); return c
hero=camera('Review_Hero',(5,-7,5.4),(0,0,.5),6.4)
close=camera('Review_Close',(1,-2,1.65),(0,0,.9),1.35)
top=camera('Review_Top',(0,0,10),(0,0,0),4.9)
bpy.ops.object.light_add(type='AREA',location=(1,-3,6)); key=bpy.context.object; key.name='Neutral_Daylight'; key.data.energy=1000; key.data.size=5; aim(key,(0,0,0))
bpy.ops.object.light_add(type='SUN',location=(-3,-4,5)); sun=bpy.context.object; sun.data.energy=2; sun.data.angle=.12; aim(sun,(0,0,0))
S.camera=hero
bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints'/f'grain_patch_v{REV}.blend'))
for label,cam in [('hero',hero),('close',close)]:
 S.camera=cam; S.render.filepath=str(P/'renders'/f'v{REV}_{label}.jpg'); bpy.ops.render.render(write_still=True)
stats={'revision':REV,'stalks':N*N,'footprint_m':[4,4],'pivot':[0,0,0],'triangles':sum(len(p.vertices)-2 for p in mesh.polygons),'vertices':len(mesh.vertices),'height_m':max(v.co.z for v in mesh.vertices),'tile_step_cm':400,'material_slots':3}
(P/f'stats_v{REV}.json').write_text(json.dumps(stats,indent=2))
print(json.dumps(stats))
