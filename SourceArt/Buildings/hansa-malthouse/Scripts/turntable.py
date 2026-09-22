import bpy,math,json
from pathlib import Path
from mathutils import Vector
J=Path(__file__).resolve().parents[1];bpy.ops.wm.open_mainfile(filepath=str(J/'exports/HansaMaltHouse_Portable.blend'));S=bpy.context.scene
S.render.resolution_x=640;S.render.resolution_y=640;S.render.resolution_percentage=100;S.eevee.taa_render_samples=32;S.render.fps=12;S.frame_start=1;S.frame_end=48
cam=S.camera;cam.data.ortho_scale=17.6
for frame in range(1,49):
 angle=-math.pi/4+(frame-1)*math.tau/48;cam.location=(25*math.cos(angle),25*math.sin(angle),14);cam.rotation_euler=(Vector((0,0,3.6))-cam.location).to_track_quat('-Z','Y').to_euler();cam.keyframe_insert(data_path='location',frame=frame);cam.keyframe_insert(data_path='rotation_euler',frame=frame)
for curve in cam.animation_data.action.fcurves:
 for key in curve.keyframe_points:key.interpolation='LINEAR'
S.render.image_settings.file_format='FFMPEG';S.render.ffmpeg.format='MPEG4';S.render.ffmpeg.codec='H264';S.render.ffmpeg.constant_rate_factor='HIGH';S.render.filepath=str(J/'renders/malthouse-turntable.mp4');bpy.ops.render.render(animation=True)
S.render.image_settings.file_format='JPEG';S.render.image_settings.quality=93
for frame in [1,13,25,37]:
 S.frame_set(frame);S.render.filepath=str(J/'renders'/('turntable-%02d.jpg'%frame));bpy.ops.render.render(write_still=True)
print('TURNTABLE_COMPLETE',48,640,640,12)
