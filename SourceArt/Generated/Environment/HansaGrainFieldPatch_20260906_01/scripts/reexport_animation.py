import bpy
from pathlib import Path
P=Path(__file__).resolve().parents[1]
S=bpy.context.scene;S.frame_set(1)
bpy.ops.object.select_all(action='DESELECT')
for o in S.objects:
 if o.type=='MESH':o.select_set(True)
ob=bpy.data.objects['SM_GrainFieldPatch_4m'];ob.data.shape_keys.animation_data.action.name='GrainWind_8s_Loop'
bpy.ops.export_scene.gltf(filepath=str(P/'exports'/'GrainFieldPatch_Animated.glb'),export_format='GLB',use_selection=True,export_animations=True,export_frame_range=True,export_morph=True,export_colors=False,export_force_sampling=False,export_nla_strips=False,export_nla_strips_merged_animation_name='GrainWind_8s_Loop')
