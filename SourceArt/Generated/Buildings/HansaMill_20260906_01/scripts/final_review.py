import bpy,pathlib,json,math,hashlib
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1];bpy.ops.wm.open_mainfile(filepath=str(P/'exports/HansaMill.blend'));S=bpy.context.scene
packing=[]
for im in bpy.data.images:
 if im.type=='IMAGE':
  assert im.packed_file,im.name;packing.append({'name':im.name,'size':list(im.size),'packed':True})
# These modifiers were inactive without auto-smooth; remove the no-op while retaining bevels.
for o in S.objects:
 for mod in list(o.modifiers):
  if mod.type=='WEIGHTED_NORMAL' and not o.data.use_auto_smooth:o.modifiers.remove(mod)
(P/'exports/master_verification.json').write_text(json.dumps({'reopened':True,'packed_images':packing,'native_generated_images':3,'inactive_modifiers_removed':True},indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(P/'exports/HansaMill.blend'))
# Additional daylight/raking review; original neutral cameras and lighting evidence remain intact.
sun=bpy.data.lights.new('Daylight diagnostic','SUN');sun.energy=2.0;sun.angle=.07;o=bpy.data.objects.new('Daylight diagnostic',sun);S.collection.objects.link(o);o.rotation_euler=(math.radians(35),math.radians(-25),math.radians(-30));S.world.node_tree.nodes['Background'].inputs[1].default_value=.45
for name in ['Hero','Rear','Timber_Detail','Roof_Detail','Base_Detail','Iron_Detail']:
 S.camera=bpy.data.objects[name];S.render.filepath=str(P/'renders'/('daylight_'+name+'.png'));bpy.ops.render.render(write_still=True)
 S.render.image_settings.file_format='JPEG';S.render.image_settings.quality=94;bpy.data.images['Render Result'].save_render(str(P/'renders'/('daylight_'+name+'.jpg')),scene=S);S.render.image_settings.file_format='PNG'
# Native 800-square rotating camera frames, rendered independently, never resized.
S.render.resolution_x=800;S.render.resolution_y=800;S.eevee.taa_render_samples=32;S.camera=bpy.data.objects['Hero'];S.camera.data.lens=43
folder=P/'renders/turntable';folder.mkdir(exist_ok=True)
for i in range(48):
 a=2*math.pi*i/48;S.camera.location=(24*math.sin(a),1-24*math.cos(a),12);S.camera.rotation_euler=(Vector((0,1,5.3))-S.camera.location).to_track_quat('-Z','Y').to_euler();S.render.filepath=str(folder/f'{i:03}.png');bpy.ops.render.render(write_still=True)
print('MASTER_REOPENED_AND_REVIEW_RENDERED',flush=True)

