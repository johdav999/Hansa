import bpy,sys,math,json
from pathlib import Path
from mathutils import Vector
P=Path(__file__).resolve().parents[1]
S=bpy.context.scene
kind=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'wind'
S.render.engine='BLENDER_EEVEE';S.eevee.taa_render_samples=16;S.eevee.use_gtao=True;S.eevee.gtao_distance=.18;S.eevee.gtao_factor=1.05
S.render.resolution_x=800;S.render.resolution_y=800;S.render.resolution_percentage=100;S.render.fps=24
S.frame_start=1;S.frame_end=192 if kind=='wind' else 96
c=bpy.data.objects['Review_Hero'];S.camera=c;c.data.ortho_scale=6.5
if kind=='turntable':
 for f in range(1,98,4):
  a=math.tau*(f-1)/96;c.location=(7*math.cos(a),7*math.sin(a),5.4);c.rotation_euler=(Vector((0,0,.5))-c.location).to_track_quat('-Z','Y').to_euler();c.keyframe_insert('location',frame=f);c.keyframe_insert('rotation_euler',frame=f)
 for fc in c.animation_data.action.fcurves:
  for k in fc.keyframe_points:k.interpolation='LINEAR'
S.render.image_settings.file_format='FFMPEG';S.render.ffmpeg.format='MPEG4';S.render.ffmpeg.codec='H264';S.render.ffmpeg.constant_rate_factor='HIGH';S.render.filepath=str(P/'renders'/(kind+'.mp4'))
bpy.ops.render.render(animation=True)
S.render.image_settings.file_format='JPEG';S.render.image_settings.quality=90
for f in [1,49,97,145] if kind=='wind' else [1,25,49,73]:
 S.frame_set(f);S.render.filepath=str(P/'renders'/(kind+'_'+str(f)+'.jpg'));bpy.ops.render.render(write_still=True)
