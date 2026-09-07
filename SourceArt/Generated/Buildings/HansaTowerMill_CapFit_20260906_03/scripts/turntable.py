import bpy,pathlib,json,math,itertools
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1];bpy.ops.wm.open_mainfile(filepath=str(P/'exports/HansaTowerMill_CapFit.blend'));S=bpy.context.scene
images=[{'name':im.name,'packed':bool(im.packed_file),'size':list(im.size)} for im in bpy.data.images if im.source=='FILE'];assert len(images)==5 and all(i['packed'] for i in images);(P/'exports/master_verification.json').write_text(json.dumps(images,indent=2))
S.render.resolution_x=800;S.render.resolution_y=800;S.eevee.taa_render_samples=32;cam=bpy.data.objects['Hero'];S.camera=cam;cam.data.type='ORTHO';cam.data.ortho_scale=23.5
bd=json.loads((P/'exports/geometry.json').read_text())['bounds_m'];corners=[Vector(v) for v in itertools.product(*[(bd[0][i],bd[1][i]) for i in range(3)])];target=Vector((0,0,(bd[0][2]+bd[1][2])*.5));checks=[];(P/'renders/turntable').mkdir(exist_ok=True)
for frame in range(48):
 a=2*math.pi*frame/48;cam.location=(35*math.sin(a),-35*math.cos(a),14);cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();bpy.context.view_layer.update();local=[cam.matrix_world.inverted()@v for v in corners];coords=[.5+v[i]/cam.data.ortho_scale for v in local for i in [0,1]];assert min(coords)>.03 and max(coords)<.97;checks.append({'frame':frame,'min':min(coords),'max':max(coords)})
 S.render.filepath=str(P/'renders/turntable'/f'{frame:03}.png');bpy.ops.render.render(write_still=True)
(P/'exports/turntable_framing.json').write_text(json.dumps(checks,indent=2));print('TURNTABLE_VERIFIED',flush=True)
