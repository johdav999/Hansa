import bpy,pathlib,math
P=pathlib.Path(__file__).resolve().parents[1];bpy.ops.wm.open_mainfile(filepath=str(P/'checkpoints/tower_r3.blend'));S=bpy.context.scene
roof=[o for o in bpy.data.collections['Roof'].objects if o.name.startswith('Roof_split_shingle')];levels=sorted(set(round(o.location.z,3) for o in roof))
for o in roof:
 if levels.index(round(o.location.z,3))%2:
  o.location.y+=.11
  if o.location.y>2.20:o.location.y=2.20;o.scale.y=.60
o=bpy.data.objects.get('Iron_hub')
if o:bpy.data.objects.remove(o,do_unlink=True)
bpy.ops.mesh.primitive_cube_add(size=1,location=(0,-3.24,10.92),rotation=(0,math.pi/4,0));o=bpy.context.object;o.name='Iron_diamond_hub_plate';o.dimensions=(.60,.17,.60);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(bpy.data.materials['ForgedIron'])
for c in list(o.users_collection):c.objects.unlink(o)
bpy.data.collections['Hardware'].objects.link(o);vc=o.data.vertex_colors.new(name='Weathering')
for v in vc.data:v.color=(.8,.8,.8,1)
mod=o.modifiers.new('Plate edge wear','BEVEL');mod.width=.012;mod.segments=2
for x,z in [(0,.27),(0,-.27),(.27,0),(-.27,0)]:
 bpy.ops.mesh.primitive_uv_sphere_add(segments=8,ring_count=4,radius=.039,location=(x,-3.35,10.92+z));o=bpy.context.object;o.name='Hub_rivet';o.data.materials.append(bpy.data.materials['ForgedIron'])
 for c in list(o.users_collection):c.objects.unlink(o)
 bpy.data.collections['Hardware'].objects.link(o);vc=o.data.vertex_colors.new(name='Weathering')
 for v in vc.data:v.color=(.8,.8,.8,1)
bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints/tower_r4.blend'))
for name in ['Front','Roof_Detail']:
 S.camera=bpy.data.objects[name];S.render.filepath=str(P/'renders'/f'r4_{name}.png');bpy.ops.render.render(write_still=True)
print('R4_COMPLETE',flush=True)
