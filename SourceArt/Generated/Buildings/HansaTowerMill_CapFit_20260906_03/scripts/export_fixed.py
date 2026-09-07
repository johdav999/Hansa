import bpy,pathlib,json,shutil
P=pathlib.Path(__file__).resolve().parents[1]
old=P.parent/'hansa-tower-mill_20260906_02'
bpy.ops.wm.open_mainfile(filepath=str(P/'exports/HansaTowerMill_CapFit.blend'))
S=bpy.context.scene
inventory=json.loads((old/'material_inventory.json').read_text())
for rec in inventory:
 for kind,path in rec['maps'].items():
  dest=P/'exports'/pathlib.Path(path).name;shutil.copy2(path,dest);rec['maps'][kind]=str(dest)
(P/'material_inventory.json').write_text(json.dumps(inventory,indent=2))
# Build standard image-based delivery materials. GLB exports COLOR_0 independently.
for rec in inventory:
 m=bpy.data.materials[rec['name']];n=m.node_tree.nodes;l=m.node_tree.links;n.clear();b=n.new('ShaderNodeBsdfPrincipled');out=n.new('ShaderNodeOutputMaterial');l.new(b.outputs[0],out.inputs[0]);b.inputs['Metallic'].default_value=.55 if rec['name']=='ForgedIron' else 0
 for kind in ['BaseColor','Roughness','Normal']:
  t=n.new('ShaderNodeTexImage');t.image=bpy.data.images.load(rec['maps'][kind],check_existing=True);t.image.colorspace_settings.name='sRGB' if kind=='BaseColor' else 'Non-Color'
  if kind=='Normal':
   normal=n.new('ShaderNodeNormalMap');l.new(t.outputs['Color'],normal.inputs['Color']);l.new(normal.outputs[0],b.inputs['Normal'])
  else:l.new(t.outputs['Color'],b.inputs['Base Color' if kind=='BaseColor' else 'Roughness'])
asset=[o for c in S.collection.children if c.name!='Review' for o in c.objects if o.type=='MESH']
bpy.ops.object.select_all(action='DESELECT')
for o in asset:o.hide_render=False;o.select_set(True)
bpy.context.view_layer.objects.active=asset[0]
# Explicit origin at ground centre; apply modifiers on export copy while preserving source master.
bpy.context.view_layer.objects.active=asset[0];bpy.ops.object.convert(target='MESH')
bpy.context.view_layer.objects.active=asset[0];bpy.ops.object.join();combined=bpy.context.object;combined.name='SM_HansaTowerMill_CapFit';S.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
points=[combined.matrix_world@v.co for v in combined.data.vertices];bounds=[[min(p[k] for p in points) for k in range(3)],[max(p[k] for p in points) for k in range(3)]]
combined.data.calc_loop_triangles();info={'bounds_m':bounds,'triangles':len(combined.data.loop_triangles),'vertices':len(combined.data.vertices),'materials':[m.name for m in combined.data.materials],'uv_layers':[u.name for u in combined.data.uv_layers],'vertex_colors':[v.name for v in combined.data.vertex_colors],'source_dimensions_inferred':True}
(P/'exports/geometry.json').write_text(json.dumps(info,indent=2))
bpy.ops.export_scene.gltf(filepath=str(P/'exports/HansaTowerMill_CapFit.glb'),export_format='GLB',use_selection=True,export_apply=True,export_colors=True)
bpy.ops.export_scene.fbx(filepath=str(P/'exports/HansaTowerMill_CapFit.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',apply_unit_scale=True,path_mode='COPY',embed_textures=False,use_mesh_modifiers=True,add_leaf_bones=False)
# Delivery scene retains vertex-color shader for direct Blender/FBX comparison.
for m in combined.data.materials:
 n=m.node_tree.nodes;l=m.node_tree.links;b=n.get('Principled BSDF');src=b.inputs['Base Color'].links[0].from_socket;vc=n.new('ShaderNodeVertexColor');vc.layer_name='Weathering';mix=n.new('ShaderNodeMixRGB');mix.blend_type='MULTIPLY';mix.inputs[0].default_value=1;l.new(src,mix.inputs[1]);l.new(vc.outputs[0],mix.inputs[2]);l.new(mix.outputs[0],b.inputs['Base Color'])
for o in bpy.data.collections['Review'].objects:o.hide_render=False
S.render.engine='BLENDER_EEVEE';S.camera=bpy.data.objects['Hero'];bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints/portable.blend'))
S.render.filepath=str(P/'renders/portable_Hero.png');bpy.ops.render.render(write_still=True)
print('EXPORT_COMPLETE',json.dumps(info),flush=True)
