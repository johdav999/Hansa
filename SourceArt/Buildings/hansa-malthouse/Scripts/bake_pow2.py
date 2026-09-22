import bpy, math, json, hashlib
from pathlib import Path
from mathutils import Vector
J=Path(__file__).resolve().parents[1];E=J/'exports';T=E/'Textures';T.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(E/'HansaMaltHouse.blend'))
# Cover cut-brick ends with full-width oak reveals, verified in export closeup.
for o in bpy.data.collections['Openings'].objects:
 if 'jamb' in o.name.lower() and 'air shutter' in o.name:
  if 'Front air shutter' in o.name or 'Rear air shutter' in o.name:o.dimensions.y=.5
  else:o.dimensions.x=.38
S=bpy.context.scene;S.render.engine='CYCLES';S.cycles.samples=1;S.cycles.device='CPU'
source_mats=[m for m in bpy.data.materials if m.name.startswith('M_MaltHouse_') and not m.name.endswith('_Portable')];records=[];export_mats={}
bpy.ops.object.select_all(action='DESELECT');bpy.ops.mesh.primitive_plane_add(size=2.5,location=(100,100,100));plane=bpy.context.object;plane.name='BakeSwatch'
for m in source_mats:
 plane.data.materials.clear();plane.data.materials.append(m);nodes=m.node_tree.nodes;links=m.node_tree.links;p=nodes.get('Principled BSDF');out=nodes.get('Material Output');maps={}
 for channel,kind in [('BaseColor','EMIT'),('Roughness','ROUGHNESS'),('NormalGL','NORMAL')]:
  im=bpy.data.images.new(m.name+'_'+channel,width=1024,height=1024,alpha=False);im.colorspace_settings.name='sRGB' if channel=='BaseColor' else 'Non-Color'
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
  check=bpy.data.images.load(str(path),check_existing=False);assert tuple(check.size)==(1024,1024);bpy.data.images.remove(check)
 em=bpy.data.materials.new(m.name+'_Portable');em.use_nodes=True;ep=em.node_tree.nodes.get('Principled BSDF');ep.inputs['Metallic'].default_value=m.get('metallic',0);el=em.node_tree.links
 for channel,inputname in [('BaseColor','Base Color'),('Roughness','Roughness'),('NormalGL','Normal')]:
  node=em.node_tree.nodes.new('ShaderNodeTexImage');node.image=bpy.data.images.load(maps[channel],check_existing=True);node.image.colorspace_settings.name='sRGB' if channel=='BaseColor' else 'Non-Color'
  if channel=='NormalGL':nm=em.node_tree.nodes.new('ShaderNodeNormalMap');el.new(node.outputs['Color'],nm.inputs['Color']);el.new(nm.outputs['Normal'],ep.inputs[inputname])
  else:el.new(node.outputs['Color'],ep.inputs[inputname])
 export_mats[m.name]=em;records.append({'material':m.name,'portable_material':em.name,'source':m.get('source','procedural'),'maps':maps,'extent_m':2.5,'baked_density_px_m':614.4,'roughness':m.get('roughness_mean'),'metallic':m.get('metallic',0),'normal_convention':'OpenGL +Y'})
bpy.data.objects.remove(plane,do_unlink=True);S.render.engine='BLENDER_EEVEE';bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(E/'HansaMaltHouse.blend'))

