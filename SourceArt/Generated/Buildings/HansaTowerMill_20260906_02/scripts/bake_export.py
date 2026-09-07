import bpy,pathlib,json,math,hashlib
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(P/'checkpoints/tower_r5.blend'))
S=bpy.context.scene
names=['Masonry','WeatheredTimber','SagePaint','OldBrick','ForgedIron','WindowGlass','RecessTimber','Fieldstone'];materials=[bpy.data.materials[n] for n in names]
for im in bpy.data.images:
 if im.source=='FILE':im.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(P/'exports/HansaTowerMill.blend'))
# Hide model only for swatch bakes, retaining editable packed source above.
for o in S.objects:o.hide_render=True
bpy.ops.mesh.primitive_plane_add(size=1);plane=bpy.context.object;plane.name='One_metre_bake_swatch';plane.hide_render=False
S.render.engine='CYCLES';S.cycles.samples=4;S.cycles.device='CPU';S.render.bake.margin=0
inventory=[]
for m in materials:
 source=m.copy();source.name=m.name+'_bake_source';plane.data.materials.clear();plane.data.materials.append(source)
 n=source.node_tree.nodes;l=source.node_tree.links;b=n.get('Principled BSDF');out=n.get('Material Output');weather=n.get('Geometry_bound_weathering')
 if weather:
  incoming=list(weather.inputs[1].links)
  if incoming:l.new(incoming[0].from_socket,b.inputs['Base Color'])
  else:
   for link in list(b.inputs['Base Color'].links):l.remove(link)
   b.inputs['Base Color'].default_value=weather.inputs[1].default_value
 rec={'name':m.name,'maps':{},'source':m.node_tree.nodes.get('ImageGen_native_color').image.filepath if m.node_tree.nodes.get('ImageGen_native_color') else 'Procedural small trim/metal/glass/recess material; no photographic texture required','uv_coverage_m':2.5 if m.name=='Masonry' else 1,'normal_convention':'OpenGL +Y','base_color_px_per_m':409.6 if m.name=='Masonry' else 1024,'physical_px_per_m':1024,'weathering':'Weathering vertex colors multiplied separately'}
 for kind,size in [('BaseColor',1024),('Roughness',1024),('Normal',1024)]:
  im=bpy.data.images.new('T_Mill_'+m.name+'_'+kind,width=size,height=size,alpha=False);im.colorspace_settings.name='sRGB' if kind=='BaseColor' else 'Non-Color'
  target=n.new('ShaderNodeTexImage');target.image=im;n.active=target
  if kind=='Normal':
   l.new(b.outputs['BSDF'],out.inputs['Surface']);bpy.ops.object.bake(type='NORMAL')
  else:
   em=n.new('ShaderNodeEmission');socket=b.inputs['Base Color' if kind=='BaseColor' else 'Roughness']
   if socket.links:l.new(socket.links[0].from_socket,em.inputs['Color'])
   else:
    val=socket.default_value;em.inputs['Color'].default_value=val if kind=='BaseColor' else (val,val,val,1)
   l.new(em.outputs[0],out.inputs['Surface']);bpy.ops.object.bake(type='EMIT');n.remove(em)
  path=P/'exports'/('T_Mill_'+m.name+'_'+kind+'.png');im.filepath_raw=str(path);im.file_format='PNG';im.save();rec['maps'][kind]=str(path);n.remove(target)
 inventory.append(rec);bpy.data.materials.remove(source);print('BAKED',m.name,flush=True)
bpy.data.objects.remove(plane,do_unlink=True)
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
bpy.context.view_layer.objects.active=asset[0];bpy.ops.object.join();combined=bpy.context.object;combined.name='SM_HansaTowerMill';S.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
points=[combined.matrix_world@v.co for v in combined.data.vertices];bounds=[[min(p[k] for p in points) for k in range(3)],[max(p[k] for p in points) for k in range(3)]]
combined.data.calc_loop_triangles();info={'bounds_m':bounds,'triangles':len(combined.data.loop_triangles),'vertices':len(combined.data.vertices),'materials':[m.name for m in combined.data.materials],'uv_layers':[u.name for u in combined.data.uv_layers],'vertex_colors':[v.name for v in combined.data.vertex_colors],'source_dimensions_inferred':True}
(P/'exports/geometry.json').write_text(json.dumps(info,indent=2))
bpy.ops.export_scene.gltf(filepath=str(P/'exports/HansaTowerMill.glb'),export_format='GLB',use_selection=True,export_apply=True,export_colors=True)
bpy.ops.export_scene.fbx(filepath=str(P/'exports/HansaTowerMill.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',apply_unit_scale=True,path_mode='COPY',embed_textures=False,use_mesh_modifiers=True,add_leaf_bones=False)
# Delivery scene retains vertex-color shader for direct Blender/FBX comparison.
for m in combined.data.materials:
 n=m.node_tree.nodes;l=m.node_tree.links;b=n.get('Principled BSDF');src=b.inputs['Base Color'].links[0].from_socket;vc=n.new('ShaderNodeVertexColor');vc.layer_name='Weathering';mix=n.new('ShaderNodeMixRGB');mix.blend_type='MULTIPLY';mix.inputs[0].default_value=1;l.new(src,mix.inputs[1]);l.new(vc.outputs[0],mix.inputs[2]);l.new(mix.outputs[0],b.inputs['Base Color'])
for o in bpy.data.collections['Review'].objects:o.hide_render=False
S.render.engine='BLENDER_EEVEE';S.camera=bpy.data.objects['Hero'];bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints/portable.blend'))
S.render.filepath=str(P/'renders/portable_Hero.png');bpy.ops.render.render(write_still=True)
print('EXPORT_COMPLETE',json.dumps(info),flush=True)
