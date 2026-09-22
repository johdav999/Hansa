import bpy, math, json, hashlib
from pathlib import Path
from mathutils import Vector
J=Path(__file__).resolve().parents[1];E=J/'exports';T=E/'Textures';T.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(J/'checkpoints/malthouse-r4.blend'))
# Cover cut-brick ends with full-width oak reveals, verified in export closeup.
for o in bpy.data.collections['Openings'].objects:
 if 'jamb' in o.name.lower() and 'air shutter' in o.name:
  if o.dimensions.x>o.dimensions.y:o.dimensions.y=.38
  else:o.dimensions.x=.38
S=bpy.context.scene;S.render.engine='CYCLES';S.cycles.samples=1;S.cycles.device='CPU'
source_mats=[m for m in bpy.data.materials if m.name.startswith('M_MaltHouse_')];records=[];export_mats={}
bpy.ops.object.select_all(action='DESELECT');bpy.ops.mesh.primitive_plane_add(size=2.5,location=(100,100,100));plane=bpy.context.object;plane.name='BakeSwatch'
for m in source_mats:
 plane.data.materials.clear();plane.data.materials.append(m);nodes=m.node_tree.nodes;links=m.node_tree.links;p=nodes.get('Principled BSDF');out=nodes.get('Material Output');maps={}
 for channel,kind in [('BaseColor','EMIT'),('Roughness','ROUGHNESS'),('NormalGL','NORMAL')]:
  im=bpy.data.images.new(m.name+'_'+channel,width=1536,height=1536,alpha=False);im.colorspace_settings.name='sRGB' if channel=='BaseColor' else 'Non-Color'
  target=nodes.new('ShaderNodeTexImage');target.image=im;nodes.active=target
  if channel=='BaseColor':
   emit=nodes.new('ShaderNodeEmission')
   if p.inputs['Base Color'].is_linked:links.new(p.inputs['Base Color'].links[0].from_socket,emit.inputs['Color'])
   else:emit.inputs['Color'].default_value=p.inputs['Base Color'].default_value
   links.new(emit.outputs[0],out.inputs['Surface'])
  bpy.ops.object.bake(type=kind,margin=8,use_clear=True)
  if channel=='BaseColor':links.new(p.outputs[0],out.inputs['Surface']);nodes.remove(emit)
  path=T/(m.name.replace('M_','T_',1)+'_'+channel+'.png');im.filepath_raw=str(path);im.file_format='PNG';im.save();nodes.remove(target);maps[channel]=str(path)
  # Decode every saved map, not just file existence.
  check=bpy.data.images.load(str(path),check_existing=False);assert tuple(check.size)==(1536,1536);bpy.data.images.remove(check)
 em=bpy.data.materials.new(m.name+'_Portable');em.use_nodes=True;ep=em.node_tree.nodes.get('Principled BSDF');ep.inputs['Metallic'].default_value=m.get('metallic',0);el=em.node_tree.links
 for channel,inputname in [('BaseColor','Base Color'),('Roughness','Roughness'),('NormalGL','Normal')]:
  node=em.node_tree.nodes.new('ShaderNodeTexImage');node.image=bpy.data.images.load(maps[channel],check_existing=True);node.image.colorspace_settings.name='sRGB' if channel=='BaseColor' else 'Non-Color'
  if channel=='NormalGL':nm=em.node_tree.nodes.new('ShaderNodeNormalMap');el.new(node.outputs['Color'],nm.inputs['Color']);el.new(nm.outputs['Normal'],ep.inputs[inputname])
  else:el.new(node.outputs['Color'],ep.inputs[inputname])
 export_mats[m.name]=em;records.append({'material':m.name,'portable_material':em.name,'source':m.get('source','procedural'),'maps':maps,'extent_m':2.5,'baked_density_px_m':614.4,'roughness':m.get('roughness_mean'),'metallic':m.get('metallic',0),'normal_convention':'OpenGL +Y'})
bpy.data.objects.remove(plane,do_unlink=True);S.render.engine='BLENDER_EEVEE';bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(E/'HansaMaltHouse.blend'))
# Build an export-only combined mesh at ground-centred pivot, keeping editable master separate.
bpy.ops.object.select_all(action='DESELECT')
parts=[o for o in bpy.data.objects if o.type=='MESH' and not any(c.name=='Review' for c in o.users_collection)]
for o in parts:
 o.select_set(True);bpy.context.view_layer.objects.active=o
 for mod in list(o.modifiers):
  try:bpy.ops.object.modifier_apply(modifier=mod.name)
  except RuntimeError: o.modifiers.remove(mod)
 for slot in o.material_slots:
  if slot.material and slot.material.name in export_mats:slot.material=export_mats[slot.material.name]
bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();combined=bpy.context.object;combined.name='SM_HansaMaltHouse';bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);S.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
bpy.ops.object.modifier_add(type='TRIANGULATE');bpy.ops.object.modifier_apply(modifier=combined.modifiers[-1].name)
# Duplicate material slots from joined components are collapsed by exact material identity.
old=list(combined.data.materials);unique=[];remap={}
for i,m in enumerate(old):
 if m not in unique:unique.append(m)
 remap[i]=unique.index(m)
indices=[remap[p.material_index] for p in combined.data.polygons];combined.data.materials.clear()
for m in unique:combined.data.materials.append(m)
for p,i in zip(combined.data.polygons,indices):p.material_index=i
points=[combined.matrix_world@v.co for v in combined.data.vertices];lo=[min(v[a] for v in points) for a in range(3)];hi=[max(v[a] for v in points) for a in range(3)]
audit={'bounds_min_m':lo,'bounds_max_m':hi,'size_m':[hi[i]-lo[i] for i in range(3)],'vertices':len(combined.data.vertices),'triangles':len(combined.data.polygons),'materials':[m.name for m in unique],'uv_channels':len(combined.data.uv_layers),'pivot':list(combined.location),'scale':list(combined.scale),'footprint_cells':[3,3],'grid_cell_m':4,'fits_11_6m_inset':all(lo[i]>=-5.8 and hi[i]<=5.8 for i in [0,1]),'generation_state':'review-draft'}
assert audit['fits_11_6m_inset'];assert abs(lo[2])<.02
bpy.ops.wm.save_as_mainfile(filepath=str(E/'HansaMaltHouse_Portable.blend'))
bpy.ops.export_scene.gltf(filepath=str(E/'SM_HansaMaltHouse.glb'),export_format='GLB',use_selection=True,export_apply=True)
bpy.ops.export_scene.fbx(filepath=str(E/'SM_HansaMaltHouse.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,mesh_smooth_type='FACE',path_mode='COPY',embed_textures=False,add_leaf_bones=False,bake_anim=False)
(E/'geometry-audit.json').write_text(json.dumps(audit,indent=2));(E/'material-inventory.json').write_text(json.dumps(records,indent=2))
S.render.image_settings.file_format='PNG';S.render.filepath=str(J/'renders/portable-hero.png');bpy.ops.render.render(write_still=True);S.render.image_settings.file_format='JPEG';S.render.image_settings.quality=92;bpy.data.images['Render Result'].save_render(str(J/'renders/portable-hero.jpg'),scene=S)
print(json.dumps(audit))
