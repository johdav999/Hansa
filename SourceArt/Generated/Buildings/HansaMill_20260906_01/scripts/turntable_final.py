import bpy,pathlib,math,sys,json,itertools
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1];bpy.ops.wm.open_mainfile(filepath=str(P/'exports/HansaMill.blend'));S=bpy.context.scene
sun=bpy.data.lights.new('Daylight diagnostic','SUN');sun.energy=2;sun.angle=.07;light=bpy.data.objects.new('Daylight diagnostic',sun);S.collection.objects.link(light);light.rotation_euler=(math.radians(35),math.radians(-25),math.radians(-30));S.world.node_tree.nodes['Background'].inputs[1].default_value=.45
S.render.resolution_x=800;S.render.resolution_y=800;S.eevee.taa_render_samples=32;cam=bpy.data.objects['Hero'];S.camera=cam;cam.data.type='ORTHO';cam.data.ortho_scale=18.5;framing=[];bounds=json.loads((P/'exports/geometry.json').read_text())['bounds_m']
frames=range(48) if '--all' in sys.argv else [24]
for i in frames:
 a=2*math.pi*i/48;cam.location=(32*math.sin(a),1-32*math.cos(a),13);cam.rotation_euler=(Vector((0,1,5.4))-cam.location).to_track_quat('-Z','Y').to_euler();bpy.context.view_layer.update();inv=cam.matrix_world.inverted();pts=[inv@Vector(v) for v in itertools.product(*[(bounds[0][j],bounds[1][j]) for j in range(3)])];screen=[(.5+v.x/18.5,.5+v.y/18.5) for v in pts];low=min(min(v) for v in screen);high=max(max(v) for v in screen);assert low>.03 and high<.97,(i,low,high);framing.append({'frame':i,'minimum_normalized':low,'maximum_normalized':high});S.render.filepath=str(P/'renders/turntable'/f'{i:03}.png');bpy.ops.render.render(write_still=True)
 if i in [0,12,24,36]:
  S.render.image_settings.file_format='JPEG';S.render.image_settings.quality=94;bpy.data.images['Render Result'].save_render(str(P/'renders'/f'turntable_{i:03}.jpg'),scene=S);S.render.image_settings.file_format='PNG'
(P/'exports/turntable_framing.json').write_text(json.dumps(framing,indent=2));print('TURNTABLE_SAFE_FRAMING',flush=True)

