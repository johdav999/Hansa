"""Import the verified yard using discovered Unreal MCP operations only."""
from pathlib import Path
import sys,json
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'SourceArt/Generated/Trees/LubeckSummer/v1/scripts'))
from ue_batch import call
A='editor_toolset.toolsets.asset.AssetTools'; M='editor_toolset.toolsets.material.MaterialTools'
O='editor_toolset.toolsets.object.ObjectTools'; T='editor_toolset.toolsets.texture.TextureTools'
S='editor_toolset.toolsets.static_mesh.StaticMeshTools'
DEST='/Game/Hansa/Generated/Staging/FirewoodModel'
JOB=ROOT/'SourceArt/Generated/Buildings/HansaWoodcutterYard_20260916/delivery'
EVIDENCE=ROOT/'Saved/GenerationJobs/Firewood_20260916/import.json'
context=json.loads(call('HansaEditor.HansaCityTerrainToolset','InspectAuthoringContext'))
assert Path(context['projectFile']).resolve()==(ROOT/'Hansa.uproject').resolve()
assert not context['pieRunning'] and all(x.startswith(DEST) for x in context['dirtyPackages']),context
# Partial import inspected in this job; resume only its staging destination.
manifest=json.loads((JOB/'manifest.json').read_text()); evidence={'project':context,'materials':[]}
def ref(path):return {'refPath':path}
def props(obj,**values):return call(O,'set_properties',instance=obj,values=json.dumps(values))
mesh_path=DEST+'/Meshes/SM_WoodcutterYard'
if call(A,'exists',path=mesh_path):mesh=ref(mesh_path+'.SM_WoodcutterYard')
else:
    meshes=call(S,'import_file',folder_path=DEST+'/Meshes',asset_name='SM_WoodcutterYard',source_file=str(JOB/'SM_WoodcutterYard.fbx'),import_materials=False,import_textures=False,combine_meshes=True)
    mesh=next(m for m in meshes if m['refPath'].endswith('.SM_WoodcutterYard'))

for mat in manifest['materials']:
    path=DEST+'/Materials/M_'+mat['source']
    material=ref(path+'.M_'+mat['source']) if call(A,'exists',path=path) else call(M,'create_material',folder_path=DEST+'/Materials',asset_name='M_'+mat['source'])
    for old in call(M,'get_expressions',material_or_function=material):call(M,'delete_expression',material_or_function=material,expression=old)
    record={'material':material,'textures':{}}
    for kind,file in mat['maps'].items():
        path=DEST+'/Textures/T_'+Path(file).stem
        texture=ref(path+'.T_'+Path(file).stem) if call(A,'exists',path=path) else call(T,'import_file',folder_path=DEST+'/Textures',asset_name='T_'+Path(file).stem,source_file=str(JOB/'textures'/file))
        if isinstance(texture,list):texture=texture[0]
        props(texture,srgb=kind=='BaseColor',**({'compressionSettings':'TC_Normalmap','bFlipGreenChannel':True} if kind=='Normal' else {}))
        node=call(M,'add_expression',material_or_function=material,expression_class=ref('/Script/Engine.MaterialExpressionTextureSample'))
        props(node,texture=texture['refPath'],**({'samplerType':'SAMPLERTYPE_Normal'} if kind=='Normal' else {'samplerType':'SAMPLERTYPE_LinearColor'} if kind=='Roughness' else {}))
        call(M,'connect_to_output',expression=node,output_name='RGB' if kind!='Roughness' else 'R',material_property={'BaseColor':'MP_BaseColor','Roughness':'MP_Roughness','Normal':'MP_Normal'}[kind])
        record['textures'][kind]=texture
    metal=call(M,'add_expression',material_or_function=material,expression_class=ref('/Script/Engine.MaterialExpressionConstant'))
    props(metal,r=mat['metallic']);call(M,'connect_to_output',expression=metal,output_name='',material_property='MP_Metallic')
    call(M,'recompile',material_or_function=material)
    call(S,'set_material',mesh=mesh,slot_name=mat['name'],material=material)
    evidence['materials'].append(record);EVIDENCE.write_text(json.dumps(evidence,indent=2));print('MATERIAL',mat['name'],flush=True)
evidence['mesh']=mesh;evidence['bounds']=call(S,'get_bounds',mesh=mesh)
evidence['slots']=call(S,'get_material_slots',mesh=mesh)
assets=call(A,'find_assets',folder_path=DEST,name='')
call(A,'save_assets',asset_paths=assets)
EVIDENCE.write_text(json.dumps(evidence,indent=2));print(json.dumps(evidence['bounds']))
