"""Measure the supplied actions without modifying the source or generating motion."""
import bpy, json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(root/'Content/Characters/Labor/Labor man 1b/laborman 1.fbx'))
rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
results={}
for action in bpy.data.actions:
    rig.animation_data.action=action
    samples=[]
    for frame in range(int(action.frame_range[0]),int(action.frame_range[1])+1):
        bpy.context.scene.frame_set(frame)
        samples.append({b:list(rig.matrix_world @ rig.pose.bones[b].matrix.translation) for b in ['root','pelvis','foot_l','foot_r','ball_l','ball_r','head']})
    results[action.name]={'seconds':(action.frame_range[1]-action.frame_range[0])/bpy.context.scene.render.fps,'first':samples[0],'last':samples[-1],
        'min':{b:[min(s[b][a] for s in samples) for a in range(3)] for b in samples[0]},'max':{b:[max(s[b][a] for s in samples) for a in range(3)] for b in samples[0]}}
    if action.name.endswith('|walk'): results[action.name]['samples']=samples
(root/'Docs/Development/AmbientPeople/animation-measurements.json').write_text(json.dumps(results,indent=2))
print(json.dumps({k:{'seconds':v['seconds'],'root_first':v['first']['root'],'root_last':v['last']['root'],'pelvis_min':v['min']['pelvis'],'pelvis_max':v['max']['pelvis'],'foot_first':v['first']['foot_l'],'toe_first':v['first']['ball_l']} for k,v in results.items()},indent=2))
