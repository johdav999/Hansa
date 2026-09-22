import bpy,json,math
from pathlib import Path
from mathutils import Vector
J=Path(__file__).resolve().parents[1];E=J/'exports';T=E/'Textures'
bpy.ops.wm.open_mainfile(filepath=str(E/'HansaMaltHouse.blend'));S=bpy.context.scene
for o in bpy.data.objects:
 if o.type=='MESH' and not any(c.name=='Review' for c in o.users_collection):
  ev=o.evaluated_get(bpy.context.evaluated_depsgraph_get()); low=min((o.matrix_world@v.co).z for v in ev.data.vertices)
  if low<-.001: print('GROUND_CORRECTION',o.name,low);o.location.z-=low
for old in list(bpy.data.materials):
 if '_Portable' in old.name:bpy.data.materials.remove(old,do_unlink=True)
export_mats={};records=[]
for m in list(bpy.data.materials):
 if not m.name.startswith('M_MaltHouse_') or m.name.endswith('_Portable'):continue
 maps={c:str(T/(m.name.replace('M_','T_',1)+'_'+c+'.png')) for c in ['BaseColor','Roughness','NormalGL']}
 em=bpy.data.materials.new(m.name+'_Portable');em.use_nodes=True;em.use_fake_user=True;ep=em.node_tree.nodes.get('Principled BSDF');ep.inputs['Metallic'].default_value=m.get('metallic',0);el=em.node_tree.links
 for c,target in [('BaseColor','Base Color'),('Roughness','Roughness'),('NormalGL','Normal')]:
  t=em.node_tree.nodes.new('ShaderNodeTexImage');t.image=bpy.data.images.load(maps[c],check_existing=False);t.image.colorspace_settings.name='sRGB' if c=='BaseColor' else 'Non-Color'
  if c=='NormalGL':nm=em.node_tree.nodes.new('ShaderNodeNormalMap');el.new(t.outputs['Color'],nm.inputs['Color']);el.new(nm.outputs['Normal'],ep.inputs[target])
  else:el.new(t.outputs['Color'],ep.inputs[target])
 export_mats[m.name]=em;records.append({'material':m.name,'portable_material':em.name,'maps':maps,'extent_m':2.5,'baked_density_px_m':409.6,'source':m.get('source','procedural'),'normal_convention':'OpenGL +Y','metallic':m.get('metallic',0)})
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(E/'HansaMaltHouse.blend'))
source=(J/'scripts/bake_export.py').read_text();tail=source[source.index('# Build an export-only'):];tail=tail.replace("assert audit['fits_11_6m_inset'];assert abs(lo[2])<.02","print('PRE_EXPORT_AUDIT',json.dumps(audit));assert audit['fits_11_6m_inset'];assert abs(lo[2])<.02")
exec(compile(tail,'finish_export_tail','exec'))

