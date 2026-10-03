"""Read the supplied FBX using Blender; retain rig and action evidence."""
import bpy, json, sys
from pathlib import Path
root = Path(__file__).resolve().parents[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(root/'Content/Characters/Labor/Labor man 1b/laborman 1.fbx'))
report = {'objects': [], 'actions': []}
for obj in bpy.data.objects:
    entry = {'name': obj.name, 'type': obj.type, 'dimensions': list(obj.dimensions), 'scale': list(obj.scale)}
    if obj.type == 'ARMATURE':
        entry['bones'] = [{'name': b.name, 'parent': b.parent.name if b.parent else None} for b in obj.data.bones]
    if obj.type == 'MESH':
        entry.update(vertices=len(obj.data.vertices), triangles=sum(len(p.vertices)-2 for p in obj.data.polygons), materials=[m.name for m in obj.data.materials])
    report['objects'].append(entry)
for action in bpy.data.actions:
    report['actions'].append({'name': action.name, 'frames': list(action.frame_range), 'curves': len(action.fcurves), 'animated_bones': sorted(set(c.data_path.split('"')[1] for c in action.fcurves if 'pose.bones[' in c.data_path))})
report['fps'] = bpy.context.scene.render.fps
out = root/'Docs/Development/AmbientPeople'
out.mkdir(parents=True, exist_ok=True)
(out/'source-inspection.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
