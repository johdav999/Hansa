import bpy,pathlib,math,json
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1];S=bpy.context.scene
ims=[i for i in bpy.data.images if i.type=='IMAGE'];assert ims and all(i.packed_file for i in ims)
(P/'exports'/'master_reopen_check.json').write_text(json.dumps({'packed_images':len(ims),'all_packed':True,'editable_source_collections':[c.name for c in S.collection.children]},indent=2))
S.render.resolution_x=960;S.render.resolution_y=960;S.render.resolution_percentage=100;S.eevee.taa_render_samples=32
cam=bpy.data.objects['Hero'];S.camera=cam;cam.data.lens=45
S.frame_start=1;S.frame_end=96;S.render.fps=24
for frame in range(1,97):
 a=-.95+(frame-1)*2*math.pi/96;cam.location=(40*math.cos(a),40*math.sin(a),22);cam.rotation_euler=(Vector((0,1,7.8))-cam.location).to_track_quat('-Z','Y').to_euler();cam.keyframe_insert(data_path='location',frame=frame);cam.keyframe_insert(data_path='rotation_euler',frame=frame)
for fc in cam.animation_data.action.fcurves:
 for kp in fc.keyframe_points:kp.interpolation='LINEAR'
S.render.image_settings.file_format='FFMPEG';S.render.ffmpeg.format='MPEG4';S.render.ffmpeg.codec='H264';S.render.ffmpeg.constant_rate_factor='HIGH';S.render.filepath=str(P/'exports'/'HansaBakery_turntable.mp4')
bpy.ops.render.render(animation=True)
S.render.image_settings.file_format='PNG'
for f in [1,25,49,73]:S.frame_set(f);S.render.filepath=str(P/'renders'/f'turntable_{f:03d}.png');bpy.ops.render.render(write_still=True)
