import bpy,json
from pathlib import Path
P=Path(__file__).resolve().parents[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(P/'exports'/'GrainFieldPatch_Animated.glb'))
S=bpy.context.scene
for o in S.objects:
 if o.type=='MESH' and o.data.shape_keys:
  print('KEYS',o.name,[(k.name,k.slider_min,k.slider_max,k.value) for k in o.data.shape_keys.key_blocks])
  print('ANIM',o.data.shape_keys.animation_data)
  for a in bpy.data.actions:
   print('ACTION',a.name,[(fc.data_path,len(fc.keyframe_points),list(fc.keyframe_points[0].co),list(fc.keyframe_points[-1].co)) for fc in a.fcurves])
  for f in [1,25,49,97,193]:
   S.frame_set(f); print('VALUES',f,[k.value for k in o.data.shape_keys.key_blocks])
