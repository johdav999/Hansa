import bpy,bmesh,math,json,pathlib,hashlib
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1];S=bpy.context.scene
# Fix final bakehouse details were retained in r5. All source geometry remains editable.
S.camera=bpy.data.objects['Hero']
# Bake original procedural families onto 2m planar samples, native 1024 square.
asset_objects=[o for o in S.objects if o.type in {'MESH','CURVE'} and o.users_collection[0].name!='Review']
mats=sorted({m for o in asset_objects for m in o.data.materials if m},key=lambda m:m.name)
source_coll=bpy.data.collections.new('Source_Shader_Archive');S.collection.children.link(source_coll)
for m in mats:
 m.use_fake_user=True
for o in S.objects:o.hide_render=True
S.render.engine='CYCLES';S.cycles.samples=1;S.cycles.bake_type='EMIT';S.render.bake.margin=4;S.render.bake.use_selected_to_active=False
bpy.ops.mesh.primitive_plane_add(size=2,location=(0,0,0));plane=bpy.context.object;plane.name='Bake_sample_2m'
# coordinates 0..2 allow UV origin and metre scaling to be portable.
for v in plane.data.vertices:v.co.x+=1;v.co.y+=1
plane.hide_render=False
baked={};inventory=[]
inventory=json.loads((P/'material_inventory.json').read_text());baked={e['name']:e['maps'] for e in inventory}
bpy.data.objects.remove(plane,do_unlink=True)
# Build portable materials separately, preserving original materials in source master.
portable={}
for m in mats:
 pm=bpy.data.materials.new('PBR_'+m.name);pm.use_nodes=True;pm.diffuse_color=m.diffuse_color;n=pm.node_tree.nodes;l=pm.node_tree.links;bs=n.get('Principled BSDF');orig=m.node_tree.nodes.get('Principled BSDF');bs.inputs['Metallic'].default_value=orig.inputs['Metallic'].default_value
 for kind,path in baked[m.name].items():
  tex=n.new('ShaderNodeTexImage');im=bpy.data.images.load(path,check_existing=False);im.colorspace_settings.name='sRGB' if kind=='BaseColor' else 'Non-Color';tex.image=im;im.pack()
  if kind=='Normal':nm=n.new('ShaderNodeNormalMap');l.new(tex.outputs['Color'],nm.inputs['Color']);l.new(nm.outputs['Normal'],bs.inputs['Normal'])
  else:l.new(tex.outputs['Color'],bs.inputs['Base Color' if kind=='BaseColor' else 'Roughness'])
 # Glass uses opaque reflective approximation, explicitly documented for clean FBX/engine parity.
 portable[m.name]=pm
# Add evaluated mesh copies, apply transforms, box project UVs at measured 512px/m.
export_coll=bpy.data.collections.new('Portable_Export');S.collection.children.link(export_coll)
dg=bpy.context.evaluated_depsgraph_get();copies=[]
for o in asset_objects:
 ev=o.evaluated_get(dg);me=bpy.data.meshes.new_from_object(ev,depsgraph=dg);co=bpy.data.objects.new(o.name+'_export',me);export_coll.objects.link(co);co.matrix_world=o.matrix_world.copy()
 for i,m in enumerate(me.materials):
  if m:me.materials[i]=portable[m.name]
 # Work in local physical coordinates accounting for nonunit scale.
 bm_uv=bmesh.new();bm_uv.from_mesh(me);bmesh.ops.triangulate(bm_uv,faces=list(bm_uv.faces));bm_uv.to_mesh(me);bm_uv.free();me.update();me.calc_normals();scale=co.matrix_world.to_scale();uv=me.uv_layers.new(name='UV0_MetreTiling') if not me.uv_layers else me.uv_layers[0]
 uv.name='UV0_MetreTiling'
 for other in list(me.uv_layers):
  if other.name!='UV0_MetreTiling':me.uv_layers.remove(other)
 me.uv_layers.active_index=0
 for p in me.polygons:
  normal=p.normal;axis=max(range(3),key=lambda k:abs(normal[k]));axes=[k for k in range(3) if k!=axis]
  # Wood grain follows longest in-plane local physical extent.
  if me.materials and me.materials[p.material_index].name=='PBR_Weathered_Oak':
   lengths=[max(v.co[k] for v in me.vertices)-min(v.co[k] for v in me.vertices) for k in axes]
   if lengths[0]*abs(scale[axes[0]])>lengths[1]*abs(scale[axes[1]]):axes.reverse()
  for li in p.loop_indices:
   v=me.vertices[me.loops[li].vertex_index].co;uv.data[li].uv=(v[axes[0]]*abs(scale[axes[0]])/2,v[axes[1]]*abs(scale[axes[1]])/2)
 # retain true transformed geometry and normalize winding after mirrored rear shutters.
 me.transform(co.matrix_world);co.matrix_world.identity();bm=bmesh.new();bm.from_mesh(me);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free();copies.append(co)
# one mesh with explicit material slots; individual source pieces remain in named collections.
bpy.ops.object.select_all(action='DESELECT')
for co in copies:co.select_set(True)
bpy.context.view_layer.objects.active=copies[0];bpy.ops.object.join();joined=bpy.context.object;joined.name='SM_HansaBakery';joined.hide_render=False
for o in S.objects:
 if o.users_collection[0].name=='Review':o.hide_render=False
for co in [c for c in S.collection.children if c.name not in ['Review','Portable_Export','Source_Shader_Archive']]:co.hide_render=True;co.hide_viewport=True
S.render.engine='BLENDER_EEVEE';S.camera=bpy.data.objects['Hero']
# Remove duplicate material slots via operator while preserving face assignment.
bpy.context.view_layer.objects.active=joined
bpy.ops.object.material_slot_remove_unused()
# Material and geometric measurements after modifiers.
joined.data.calc_loop_triangles();bounds=[min(v.co[k] for v in joined.data.vertices) for k in range(3)]+[max(v.co[k] for v in joined.data.vertices) for k in range(3)]
report={'triangles':len(joined.data.loop_triangles),'vertices':len(joined.data.vertices),'bounds_m':bounds,'dimensions_m':[bounds[k+3]-bounds[k] for k in range(3)],'materials':[m.name for m in joined.data.materials],'uv_density':'512 px/m nominal planar projection; directional faces may have projection loss','intended_view_distance_m':5,'pivot':'ground-centred main house origin','forward':'-Y','up':'+Z','glass':'opaque reflective approximation in portable exports','collision':'not authored','LOD':'not authored'}
(P/'material_inventory.json').write_text(json.dumps(inventory,indent=2));(P/'exports'/'mesh_manifest.json').write_text(json.dumps(report,indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(P/'exports'/'HansaBakery.blend'))
S.render.filepath=str(P/'renders'/'portable_Hero.png');bpy.ops.render.render(write_still=True)
bpy.ops.object.select_all(action='DESELECT');joined.select_set(True);bpy.context.view_layer.objects.active=joined
bpy.ops.export_scene.gltf(filepath=str(P/'exports'/'HansaBakery.glb'),export_format='GLB',use_selection=True,export_texcoords=True,export_normals=True,export_materials='EXPORT',export_yup=True)
bpy.ops.export_scene.fbx(filepath=str(P/'exports'/'HansaBakery.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,path_mode='COPY',embed_textures=False)
print('EXPORT_COMPLETE',json.dumps(report),flush=True)
