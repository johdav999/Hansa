import bpy,pathlib,random,math
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1];random.seed(744)
bpy.ops.wm.open_mainfile(filepath=str(P/'checkpoints/tower_r2.blend'));S=bpy.context.scene
for o in bpy.data.collections['Tower'].objects:
 if not o.name.startswith('Exposed_fieldstone'):continue
 normal=Vector((o.location.x,o.location.y,0)).normalized();o.location-=normal*.065
 tone=random.uniform(.50,.79)
 for v in o.data.vertex_colors['Weathering'].data:v.color=(tone,tone*.96,tone*.91,1)
 uv=o.data.uv_layers.active;offset=(random.random(),random.random())
 for f in o.data.polygons:
  for li in f.loop_indices:v=o.data.vertices[o.data.loops[li].vertex_index].co;uv.data[li].uv=(v.x+offset[0],v.z+offset[1])
m=bpy.data.materials['OldBrick'];n=m.node_tree.nodes;l=m.node_tree.links;t=n.new('ShaderNodeTexImage');t.name='ImageGen_native_color';t.image=bpy.data.images.load(str(P/'textures/tower--brick--worn--1254x1254--v1.png'));l.new(t.outputs[0],n['Geometry_bound_weathering'].inputs[1])
for o in bpy.data.collections['Openings'].objects:
 if o.name.startswith('Brick_'):
  for u in o.data.uv_layers.active.data:u.uv*=2
  tone=random.uniform(.55,.83)
  for v in o.data.vertex_colors['Weathering'].data:v.color=(tone,tone*.95,tone*.88,1)
# Localized sill runoff and irregular lower-wall dampness on subdivided geometry.
o=bpy.data.objects['Tapered_masonry_tower'];bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.subdivide(number_cuts=5,smoothness=0);bpy.ops.object.mode_set(mode='OBJECT')
vc=o.data.vertex_colors['Weathering']
for f in o.data.polygons:
 for li in f.loop_indices:
  p=o.matrix_world@o.data.vertices[o.data.loops[li].vertex_index].co
  damp=max(0,1-p.z/1.15)*(.10+.12*math.sin(p.x*1.3+p.y*.8)**2)
  runoff=0
  if p.y<-.5:
   for sill,width in [(3.3,.5),(5.35,.42),(7.15,.3)]:
    below=sill-p.z
    if 0<below<1.3:runoff+=math.exp(-(p.x/(width*(1+.15*below)))**2)*(1-below/1.3)*.13
  tone=.98*(1-damp-runoff);vc.data[li].color=(tone,tone*.96,tone*.90,1)
for name in ['Sails','Roof']:
 for ob in bpy.data.collections[name].objects:
  if ob.type!='MESH' or not ob.data.vertex_colors:continue
  tone=random.uniform(.44,.61) if name=='Sails' else random.uniform(.55,.70)
  for v in ob.data.vertex_colors['Weathering'].data:v.color=(tone*.96,tone,tone*1.025,1)
# Additional pegged joints and iron fasteners are actual geometry.
for k in range(4):
 a=math.pi/4+k*math.pi/2;d=Vector((math.sin(a),0,math.cos(a)));t=Vector((math.cos(a),0,-math.sin(a)))
 for r in [.5,.95,1.4,3.8,6.4]:
  p=Vector((0,-2.80,10.92))+d*r
  bpy.ops.mesh.primitive_cylinder_add(vertices=8,radius=.029,depth=.035,location=p,rotation=(math.pi/2,0,0));o=bpy.context.object;o.name='Sail_iron_bolt';o.data.materials.append(bpy.data.materials['ForgedIron'])
  for c in list(o.users_collection):c.objects.unlink(o)
  bpy.data.collections['Hardware'].objects.link(o);vc=o.data.vertex_colors.new(name='Weathering')
  for v in vc.data:v.color=(.8,.8,.8,1)
bpy.ops.mesh.primitive_cube_add(size=1,location=(0,-3.30,.025));o=bpy.context.object;o.name='Entry_floor_continuity';o.dimensions=(1.6,1.5,.08);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(bpy.data.materials['RecessTimber'])
for c in list(o.users_collection):c.objects.unlink(o)
bpy.data.collections['Tower'].objects.link(o);vc=o.data.vertex_colors.new(name='Weathering')
for v in vc.data:v.color=(.8,.8,.8,1)
for im in bpy.data.images:
 if im.source=='FILE':im.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints/tower_r3.blend'))
for name in ['Front','Hero','Rear','Base_Detail','Roof_Detail','Iron_Detail']:
 S.camera=bpy.data.objects[name];S.render.filepath=str(P/'renders'/f'r3_{name}.png');bpy.ops.render.render(write_still=True)
print('R3_COMPLETE',flush=True)
