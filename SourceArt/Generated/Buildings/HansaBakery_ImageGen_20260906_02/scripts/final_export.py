
import bpy,json,pathlib,hashlib
P=pathlib.Path(__file__).resolve().parents[1];S=bpy.context.scene
bpy.context.preferences.filepaths.save_version=0
o=bpy.data.objects['SM_HansaBakery'];me=o.data;uv=me.uv_layers.active
# Offset each disconnected oak member coherently; preserve vertical grain and all-channel registration.
wood=[p for p in me.polygons if me.materials[p.material_index].name=='PBR_Weathered_Oak']
vpolys={}
for p in wood:
 for v in p.vertices:vpolys.setdefault(v,[]).append(p.index)
pending={p.index for p in wood};groups=0
while pending:
 start=min(pending);stack=[start];group=[];pending.remove(start)
 while stack:
  pi=stack.pop();group.append(pi)
  for v in me.polygons[pi].vertices:
   for nb in vpolys[v]:
    if nb in pending:pending.remove(nb);stack.append(nb)
 h=hashlib.sha256(str(start).encode()).digest();offset=(h[0]/255,h[1]/255)
 for pi in group:
  for li in me.polygons[pi].loop_indices:uv.data[li].uv.x+=offset[0];uv.data[li].uv.y+=offset[1]
 groups+=1
(P/'evidence/oak_phase_correction.json').write_text(json.dumps({'members':groups,'reason':'Reduce identical knot phase across separate timber members','orientation':'preserved','maps':'same UV phase for color/roughness/normal'},indent=2))
S.camera=bpy.data.objects['Hero']
bpy.ops.wm.save_as_mainfile(filepath=str(P/'exports/HansaBakery.blend'))
for view in ['Hero','Detail_Shop','Detail_Roof']:
 S.camera=bpy.data.objects[view];S.render.filepath=str(P/'renders'/('final_'+view+'.png'));bpy.ops.render.render(write_still=True)
bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
bpy.ops.export_scene.gltf(filepath=str(P/'exports/HansaBakery.glb'),export_format='GLB',use_selection=True,export_texcoords=True,export_normals=True,export_materials='EXPORT',export_yup=True)
bpy.ops.export_scene.fbx(filepath=str(P/'exports/HansaBakery.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,path_mode='COPY',embed_textures=False)
print('EXPORTS_COMPLETE',groups,flush=True)

