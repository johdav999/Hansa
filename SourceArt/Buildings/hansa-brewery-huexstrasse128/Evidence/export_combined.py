"""Apply modifiers and combine before FBX export to preserve global material indices."""
import bpy,json
from pathlib import Path
JOB=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(JOB/"checkpoints/hansa-brewery-huexstrasse128-r5.blend"))
bpy.ops.object.select_all(action="DESELECT")
parts=[o for o in bpy.data.objects if o.type=="MESH" and o.name!="ReviewGround"]
for o in parts:o.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.object.convert(target="MESH")
bpy.ops.object.join()
combined=bpy.context.object
combined.name="SM_HansaBrewery_Production"
combined.location=tuple(v*100 for v in combined.location)
combined.scale=tuple(v*100 for v in combined.scale)
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
materials=[m.name for m in combined.data.materials]
assert len(materials)==16,materials
assert max(p.material_index for p in combined.data.polygons)<16
target=JOB/"exports/SM_HansaBrewery_Production_Combined.fbx"
bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,global_scale=1.0,apply_unit_scale=False,apply_scale_options="FBX_SCALE_ALL",axis_forward="-Y",axis_up="Z",use_mesh_modifiers=True,mesh_smooth_type="FACE",add_leaf_bones=False,bake_anim=False,path_mode="COPY",embed_textures=False)
(JOB/"combined-export.json").write_text(json.dumps({"materials":materials,"polygons":len(combined.data.polygons),"export":str(target)},indent=2))
