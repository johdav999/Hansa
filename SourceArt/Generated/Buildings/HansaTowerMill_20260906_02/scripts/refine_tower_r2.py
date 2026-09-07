import bpy,math,random,pathlib
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1];random.seed(882)
bpy.ops.wm.open_mainfile(filepath=str(P/'checkpoints/tower_r1_repaired.blend'));S=bpy.context.scene
print('OPEN_OK',flush=True)
# Proper wedge-shaped voussoirs replace the conspicuously stepped box arch.
for o in list(bpy.data.objects):
 if o.name.startswith('Brick_arch'):bpy.data.objects.remove(o,do_unlink=True)
brick=bpy.data.materials['OldBrick'];brick.node_tree.nodes['Geometry_bound_weathering'].inputs[1].default_value=(.20,.074,.043,1)
def radius(z):return 4-1.45*z/8.7
for lo,sh,rise,w in [(.05,1.85,.32,1.65),(3.35,4.08,.19,.72),(5.4,6,.15,.59),(7.2,7.65,.13,.43)]:
 for j in range(15):
  a=math.pi*j/15+.012;b=math.pi*(j+1)/15-.012
  p=[(w/2*math.cos(a),sh+rise*math.sin(a)),(w/2*math.cos(b),sh+rise*math.sin(b)),((w/2+.18)*math.cos(b),sh+(rise+.19)*math.sin(b)),((w/2+.18)*math.cos(a),sh+(rise+.19)*math.sin(a))]
  verts=[(x,-radius(z)+depth,z) for depth in [-.10,.16] for x,z in p];faces=[(3,2,1,0),(4,5,6,7)]+[(i,(i+1)%4,(i+1)%4+4,i+4) for i in range(4)]
  me=bpy.data.meshes.new('Wedge');me.from_pydata(verts,[],faces);me.update();o=bpy.data.objects.new('Brick_arch_voussoir',me);bpy.data.collections['Openings'].objects.link(o);me.materials.append(brick)
  uv=me.uv_layers.new(name='SurfaceMetres');vc=me.vertex_colors.new(name='Weathering');uv=me.uv_layers['SurfaceMetres'];tone=random.uniform(.72,1.02)
  for f in me.polygons:
   for li in f.loop_indices:v=me.vertices[me.loops[li].vertex_index].co;uv.data[li].uv=(v.x,v.z);vc.data[li].color=(tone,tone,tone,1)
  me.validate();me.update();mod=o.modifiers.new('Chipped brick edge','BEVEL');mod.width=.006;mod.segments=2
print('ARCHES_OK',flush=True)
# Individually modeled partially exposed fieldstones break the perfectly smooth render.
stone=bpy.data.materials.new('Fieldstone');stone.use_nodes=True;n=stone.node_tree.nodes;l=stone.node_tree.links;b=n.get('Principled BSDF');b.inputs['Roughness'].default_value=.91
t=n.new('ShaderNodeTexImage');t.name='ImageGen_native_color';t.image=bpy.data.images.load(str(P/'textures/tower--stone--worn--1254x1254--v1.png'));vc=n.new('ShaderNodeVertexColor');vc.layer_name='Weathering';mix=n.new('ShaderNodeMixRGB');mix.name='Geometry_bound_weathering';mix.blend_type='MULTIPLY';mix.inputs[0].default_value=1;l.new(t.outputs[0],mix.inputs[1]);l.new(vc.outputs[0],mix.inputs[2]);l.new(mix.outputs[0],b.inputs['Base Color']);no=n.new('ShaderNodeTexNoise');no.inputs['Scale'].default_value=100;bu=n.new('ShaderNodeBump');bu.inputs['Distance'].default_value=.002;bu.inputs['Strength'].default_value=.25;l.new(no.outputs['Fac'],bu.inputs['Height']);l.new(bu.outputs[0],b.inputs['Normal'])
for k in range(470):
 z=random.uniform(.13,8.56);a=random.uniform(-math.pi,math.pi);r=radius(z);x=r*math.sin(a);y=-r*math.cos(a)
 # Clear all architectural reveals; vary density by broad, irregular failure patches.
 if y<0 and abs(x)<1.16 and any(lo-.28<z<hi+.28 for lo,hi in [(.0,2.25),(3.25,4.45),(5.25,6.3),(7.07,7.95)]):continue
 if math.sin(a*4+z*1.2)+math.sin(z*3-a*2)<-.45:continue
 bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=1,location=(x,y,z));o=bpy.context.object;o.name='Exposed_fieldstone';o.scale=(random.uniform(.10,.27),random.uniform(.055,.095),random.uniform(.08,.19));bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 for v in o.data.vertices:v.co+=Vector([random.uniform(-.018,.018) for _ in range(3)])
 o.rotation_euler[2]=a;o.data.materials.append(stone)
 for c in list(o.users_collection):c.objects.unlink(o)
 bpy.data.collections['Tower'].objects.link(o);uv=o.data.uv_layers.new(name='SurfaceMetres');vc=o.data.vertex_colors.new(name='Weathering');uv=o.data.uv_layers['SurfaceMetres'];tone=random.uniform(.32,.90);warm=random.uniform(.80,1.0)
 for f in o.data.polygons:
  for li in f.loop_indices:v=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(v.x+random.random(),v.z+random.random());vc.data[li].color=(tone,tone*warm,tone*warm*.9,1)
 mod=o.modifiers.new('Soft chipped corners','BEVEL');mod.width=.015;mod.segments=2
print('STONES_OK',flush=True)
# Remove the pale cap bottom seam and darken sheltered eave edge.
for o in bpy.data.collections['Roof'].objects:
 if o.name.startswith('Cap_front_shingle') and o.location.z<9.15:
  for v in o.data.vertex_colors['Weathering'].data:v.color=(.42,.43,.44,1)
bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints/tower_r2.blend'))
for name in ['Front','Base_Detail']:
 S.camera=bpy.data.objects[name];S.render.filepath=str(P/'renders'/f'r2_{name}.png');bpy.ops.render.render(write_still=True)
print('R2_COMPLETE')
