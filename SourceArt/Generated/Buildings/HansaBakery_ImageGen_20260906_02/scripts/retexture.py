
import bpy,json,pathlib,os,hashlib
P=pathlib.Path(__file__).resolve().parents[1];S=bpy.context.scene
bpy.context.preferences.filepaths.save_version=0
inv=json.loads((P/'material_inventory.json').read_text())
model=bpy.data.objects['SM_HansaBakery']
spec={}
for i in range(3):
 spec['Brick_'+str(i)]=('brick',[(.92,.85,.83),(1.06,1.00,.94),(.78,.77,.78)][i],4)
 spec['Clay_'+str(i)]=('clay',[(.88,.76,.72),(1,.9,.82),(.72,.68,.64)][i],4)
spec.update({'Lime_Plaster':('plaster',(1,1,1),1),'Damp_Lime':('plaster',(.73,.74,.70),1),'Weathered_Oak':('oak',(.78,.74,.69),1)})
# Geometry remains identical; map only brick/clay face detail to 0.5m physical coverage.
for p in model.data.polygons:
 name=model.data.materials[p.material_index].name.removeprefix('PBR_')
 if name in spec and spec[name][2]!=1:
  for li in p.loop_indices:model.data.uv_layers.active.data[li].uv*=spec[name][2]
# Retain a named editable hybrid source per material, bake its color at the ImageGen native size.
visibility={o.name:o.hide_render for o in S.objects}
for o in S.objects:o.hide_render=True
bpy.ops.mesh.primitive_plane_add(size=2);plane=bpy.context.object;plane.name='HybridBakeSample'
S.render.engine='CYCLES';S.cycles.samples=1;S.render.bake.margin=0;S.render.bake.use_selected_to_active=False
updated=[]
for entry in inv:
 name=entry['name'];pm=bpy.data.materials['PBR_'+name]
 for kind in list(entry['maps']):entry['maps'][kind]=str(P/'textures'/pathlib.Path(entry['maps'][kind]).name)
 # Reload retained maps from revision copies, preserving source inputs.
 for n in pm.node_tree.nodes:
  if n.type=='TEX_IMAGE' and n.image:
   kind=next((k for k in entry['maps'] if k in n.image.name),None)
   if kind:
    n.image=bpy.data.images.load(entry['maps'][kind],check_existing=True);n.image.colorspace_settings.name='sRGB' if kind=='BaseColor' else 'Non-Color'
 if name not in spec:entry['revision']='retained procedural material; outside the four primary facade/roof/timber families';continue
 family,tint,uvscale=spec[name]
 src=next((P/'texture_sources').glob('bakery--'+family+'--*.png'))
 im=bpy.data.images.load(str(src),check_existing=True);im.colorspace_settings.name='sRGB';im.pack()
 hybrid=bpy.data.materials.new('HYBRID_ImageGen_'+name);hybrid.use_nodes=True;hybrid.use_fake_user=True
 n=hybrid.node_tree.nodes;l=hybrid.node_tree.links;bs=n.get('Principled BSDF');out=n.get('Material Output')
 tex=n.new('ShaderNodeTexImage');tex.name='ImageGen_native_color';tex.label=family+' native source';tex.image=im
 tintnode=n.new('ShaderNodeMixRGB');tintnode.name='Material_tint';tintnode.blend_type='MULTIPLY';tintnode.inputs[0].default_value=1;tintnode.inputs[2].default_value=(*tint,1);l.new(tex.outputs['Color'],tintnode.inputs[1]);l.new(tintnode.outputs[0],bs.inputs['Base Color'])
 # Preserve physical channels independently from generated image luminance.
 for kind in ['Roughness','Normal']:
  t=n.new('ShaderNodeTexImage');t.name='Procedural_'+kind;t.image=bpy.data.images.load(entry['maps'][kind],check_existing=True);t.image.colorspace_settings.name='Non-Color'
  if kind=='Normal':
   nm=n.new('ShaderNodeNormalMap');l.new(t.outputs['Color'],nm.inputs['Color']);l.new(nm.outputs[0],bs.inputs['Normal'])
  else:l.new(t.outputs[0],bs.inputs['Roughness'])
 plane.data.materials.clear();plane.data.materials.append(hybrid)
 em=n.new('ShaderNodeEmission');l.new(tintnode.outputs[0],em.inputs[0]);l.new(em.outputs[0],out.inputs['Surface'])
 target=bpy.data.images.new(name+'_ImageGen_BaseColor',width=im.size[0],height=im.size[1],alpha=False);target.colorspace_settings.name='sRGB'
 bake=n.new('ShaderNodeTexImage');bake.image=target;n.active=bake
 bpy.context.view_layer.objects.active=plane;plane.select_set(True);bpy.ops.object.bake(type='EMIT')
 dest=P/'textures'/(name+'_ImageGen_BaseColor.png');target.filepath_raw=str(dest);target.file_format='PNG';target.save()
 l.new(bs.outputs[0],out.inputs['Surface']);n.remove(em);n.remove(bake)
 pbs=pm.node_tree.nodes.get('Principled BSDF')
 oldtex=pbs.inputs['Base Color'].links[0].from_node;oldtex.image=target
 entry['maps']['BaseColor']=str(dest);entry['source']='ImageGen color with material tint; retained procedural roughness/normal, not image-luminance derived'
 entry['imagegen_source']=str(src);entry['color_dimensions']=list(im.size);entry['extent_m']=[2/uvscale]*2;entry['normal_roughness_dimensions']=[1024,1024];entry['tint_linear']=tint
 entry['nominal_density_px_m']={'color':im.size[0]/(2/uvscale),'physical_channels':1024/(2/uvscale)}
 updated.append(name);print('HYBRID_BAKED',name,flush=True)
bpy.data.objects.remove(plane,do_unlink=True)
for o in S.objects:o.hide_render=visibility.get(o.name,False)
for im in bpy.data.images:
 if im.type=='IMAGE' and im.source=='FILE' and im.has_data:
  try:im.pack()
  except:pass
S.render.engine='BLENDER_EEVEE';S.camera=bpy.data.objects['Hero']
(P/'material_inventory.json').write_text(json.dumps(inv,indent=2))
model.data.calc_loop_triangles()
(P/'evidence/revision_geometry.json').write_text(json.dumps({'vertices':len(model.data.vertices),'triangles':len(model.data.loop_triangles),'updated_materials':updated,'geometry_change':'none; brick/clay UV coverage changed to 0.5m'},indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(P/'exports/HansaBakery.blend'))
for view in ['Hero','Detail_Shop','Detail_Roof']:
 S.camera=bpy.data.objects[view];S.render.filepath=str(P/'renders'/('imagegen_v1_'+view+'.png'));bpy.ops.render.render(write_still=True)
# Repeated material swatches: 4 repeats across the plane, under existing review daylight.
for o in S.objects:
 if o.type=='MESH':o.hide_render=True
bpy.ops.mesh.primitive_plane_add(size=4,location=(0,0,10));sw=bpy.context.object
for uv in sw.data.uv_layers.active.data:uv.uv*=4
from mathutils import Vector
S.camera=bpy.data.objects['Hero'];S.camera.data.type='ORTHO';S.camera.data.ortho_scale=4.3;S.camera.location=(0,0,15);S.camera.rotation_euler=(0,0,0)
S.render.resolution_x=1254;S.render.resolution_y=1254
for name in ['Lime_Plaster','Brick_0','Clay_0','Weathered_Oak']:
 sw.data.materials.clear();sw.data.materials.append(bpy.data.materials['PBR_'+name]);S.render.filepath=str(P/'renders'/('swatch_'+name+'.png'));bpy.ops.render.render(write_still=True)
print('REVISION_RENDER_COMPLETE',flush=True)

