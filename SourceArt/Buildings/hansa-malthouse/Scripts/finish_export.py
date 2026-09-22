import bpy,json,math
from pathlib import Path
from mathutils import Vector
J=Path(__file__).resolve().parents[1];E=J/'exports';T=E/'Textures'
bpy.ops.wm.open_mainfile(filepath=str(E/'HansaMaltHouse.blend'));S=bpy.context.scene
for o in bpy.data.objects:
 if o.name.startswith('Service jamb'):o.location.z+=.03
export_mats={m.name:m2 for m in bpy.data.materials for m2 in bpy.data.materials if m.name.startswith('M_MaltHouse_') and not m.name.endswith('_Portable') and m2.name==m.name+'_Portable'}
records=[{'material':n,'portable_material':m.name,'maps':{c:str(T/(n.replace('M_','T_',1)+'_'+c+'.png')) for c in ['BaseColor','Roughness','NormalGL']},'extent_m':2.5,'baked_density_px_m':614.4,'source':bpy.data.materials[n].get('source','procedural'),'normal_convention':'OpenGL +Y','metallic':bpy.data.materials[n].get('metallic',0)} for n,m in export_mats.items()]
bpy.ops.wm.save_as_mainfile(filepath=str(E/'HansaMaltHouse.blend'))
source=(J/'scripts/bake_export.py').read_text();tail=source[source.index('# Build an export-only'):];tail=tail.replace("assert audit['fits_11_6m_inset'];assert abs(lo[2])<.02","print('PRE_EXPORT_AUDIT',json.dumps(audit));assert audit['fits_11_6m_inset'];assert abs(lo[2])<.02")
exec(compile(tail,'finish_export_tail','exec'))
