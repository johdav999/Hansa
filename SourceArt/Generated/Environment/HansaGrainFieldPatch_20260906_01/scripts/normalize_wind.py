import bpy,math,json
from pathlib import Path
from mathutils import Vector
P=Path(__file__).resolve().parents[1];reports=[]
for name in ['GrainFieldPatch_Animated_Source.blend','GrainFieldPatch_Animated_Portable.blend']:
 bpy.ops.wm.open_mainfile(filepath=str(P/'exports'/name));S=bpy.context.scene;ob=bpy.data.objects['SM_GrainFieldPatch_4m'];keys=ob.data.shape_keys
 def sample(f):
  S.frame_set(f);ev=ob.evaluated_get(bpy.context.evaluated_depsgraph_get());me=ev.to_mesh();co=[me.vertices[i].co.copy() for i in range(0,len(me.vertices),500)];ev.to_mesh_clear();return co
 frames=[1,49,97,145,193];before=[sample(f) for f in frames]
 basis,a,b=keys.key_blocks
 for i in range(len(basis.data)):
  origin=basis.data[i].co.copy();da=a.data[i].co-origin;db=b.data[i].co-origin
  basis.data[i].co=origin-da-db;a.data[i].co=origin+da-db;b.data[i].co=origin-da+db
 for k in [a,b]:k.slider_min=0;k.slider_max=1
 for fc in keys.animation_data.action.fcurves:
  for kp in fc.keyframe_points:
   kp.co.y=.5+.5*kp.co.y
   kp.handle_left.y=.5+.5*kp.handle_left.y;kp.handle_right.y=.5+.5*kp.handle_right.y
 keys.animation_data.action.name='GrainWind_8s_Loop'
 after=[sample(f) for f in frames];err=max((v-w).length for aa,bb in zip(before,after) for v,w in zip(aa,bb));assert err<.000002
 S.frame_set(1);bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(P/'exports'/name));reports.append({'master':name,'sampled_pose_equivalence_error_m':err,'morph_weight_range':[0,1]})
bpy.ops.object.select_all(action='DESELECT')
for o in S.objects:
 if o.type=='MESH':o.select_set(True)
bpy.ops.export_scene.gltf(filepath=str(P/'exports'/'GrainFieldPatch_Animated.glb'),export_format='GLB',use_selection=True,export_animations=True,export_frame_range=True,export_morph=True,export_colors=False,export_force_sampling=False,export_nla_strips=False,export_nla_strips_merged_animation_name='GrainWind_8s_Loop')
(P/'wind_normalization.json').write_text(json.dumps(reports,indent=2))
