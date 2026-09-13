"""Task-bounded P18 import. Requires the previously inspected empty material draft."""
import json,sys,time
from pathlib import Path
REPO=Path(__file__).resolve().parents[1];JOB=REPO/'Saved/GenerationJobs/hansa-road_P18_20260908'
sys.path.insert(0,str(REPO/'SourceArt/Generated/Buildings/HansaSawmill_P13_20260908/scripts'))
from ue_batch import call
A='editor_toolset.toolsets.asset.AssetTools';S='editor_toolset.toolsets.static_mesh.StaticMeshTools';M='editor_toolset.toolsets.material.MaterialTools';T='editor_toolset.toolsets.texture.TextureTools';O='editor_toolset.toolsets.object.ObjectTools';B='editor_toolset.toolsets.blueprint.BlueprintTools';SC='editor_toolset.toolsets.scene.SceneTools'
ROOT='/Game/Hansa/Generated/Staging/Road_P18'
assert call(SC,'get_current_level')=='/Game/Hansa/Generated/Staging/Harbor_P17/L_Harbor_Review'
assert not call(A,'is_dirty',asset_path=call(SC,'get_current_level'))
manifest=json.loads((JOB/'exports/export-manifest.json').read_text());nodes=json.loads((JOB/'evidence/material-nodes.json').read_text())
mat={'refPath':ROOT+'/Materials/M_Road_Earth.M_Road_Earth'}
def props(o,v):assert call(O,'set_properties',instance=o,values=json.dumps(v))
expression_indices={'Multiply':1,'LinearInterpolate':1,'TextureSample':1}
existing_expressions=call(M,'get_expressions',material_or_function=mat)
def expression(name):
    index=expression_indices.get(name,0);expression_indices[name]=index+1
    existing=next((e for e in existing_expressions if e['refPath'].endswith(':MaterialExpression'+name+'_'+str(index))),None)
    if existing:return existing
    cls=next(c for c in call(M,'list_expression_classes',material_or_function=mat,search=name) if c['refPath'].endswith('.MaterialExpression'+name))
    return call(M,'add_expression',material_or_function=mat,expression_class=cls)
def link(a,out,b,pin):call(M,'connect_expressions',from_expression=a,from_output_name=out,to_expression=b,to_input_name=pin)
props(mat,{'blendMode':'BLEND_Masked','ditherOpacityMask':True,'ditheredLODTransition':True})
props(nodes['ComponentMask'],{'r':True,'g':True,'b':False,'a':False});props(nodes['Multiply'],{'constB':.005})
link(nodes['WorldPosition'],'',nodes['ComponentMask'],'');link(nodes['ComponentMask'],'',nodes['Multiply'],'A')
props(nodes['ScalarParameter'],{'parameterName':'Wetness','defaultValue':0,'sliderMin':0,'sliderMax':1})
props(nodes['LinearInterpolate'],{'constA':1,'constB':.48});link(nodes['ScalarParameter'],'',nodes['LinearInterpolate'],'Alpha')
textures=[]
for i,channel in enumerate(('BaseColor','Roughness','Normal')):
    name='T_Road_'+channel
    path=ROOT+'/Textures/'+name
    if call(A,'exists',path=path):
        # Resume only this task's three dedicated texture paths; never overwrite.
        tex={'refPath':path+'.'+name}
    else:
        tex=call(T,'import_file',folder_path=ROOT+'/Textures',asset_name=name,source_file=str(JOB/'exports'/('M_Road_Earth_'+channel+'.png')))[0]
    props(tex,{'sRGB':channel=='BaseColor','compressionSettings':{'BaseColor':'TC_Default','Roughness':'TC_Masks','Normal':'TC_Normalmap'}[channel],'bFlipGreenChannel':channel=='Normal'})
    call(A,'save_assets',asset_paths=[tex['refPath']])
    for attempt in range(20):
        size=call(T,'get_size',texture=tex)
        if size=={'x':1024,'y':1024}:break
        time.sleep(.25)
    assert size=={'x':1024,'y':1024},(name,size)
    e=nodes['TextureSample'] if i==0 else expression('TextureSample')
    props(e,{'texture':tex,'samplerType':{'BaseColor':'SAMPLERTYPE_Color','Roughness':'SAMPLERTYPE_Masks','Normal':'SAMPLERTYPE_Normal'}[channel]});link(nodes['Multiply'],'',e,'UVs')
    if channel!='Normal':
        multiply=expression('Multiply');link(e,'',multiply,'A')
        if channel=='BaseColor':factor=nodes['LinearInterpolate']
        else:
            factor=expression('LinearInterpolate');props(factor,{'constA':1,'constB':.55});link(nodes['ScalarParameter'],'',factor,'Alpha')
        link(factor,'',multiply,'B');e=multiply
    call(M,'connect_to_output',expression=e,output_name='',material_property='MP_'+channel)
    call(A,'save_assets',asset_paths=[tex['refPath']]);textures.append(tex)
call(M,'connect_to_output',expression=nodes['VertexColor'],output_name='A',material_property='MP_OpacityMask')
call(M,'recompile',material_or_function=mat);call(A,'save_assets',asset_paths=[mat['refPath']])
records={}
for name,info in manifest['modules'].items():
    assert not call(A,'exists',path=ROOT+'/Meshes/'+name)
    mesh=call(S,'import_file',folder_path=ROOT+'/Meshes',asset_name=name,source_file=str(JOB/'exports'/info['fbx']),import_materials=False,import_textures=False,combine_meshes=True)[0]
    bounds=call(S,'get_bounds',mesh=mesh)
    assert all(bounds['min'][axis]>=-200.02 and bounds['max'][axis]<=200.02 for axis in ('x','y')),(name,bounds)
    assert call(S,'generate_lods',mesh=mesh,triangle_percents=[.5,.2])==3
    call(S,'set_lod_thresholds',mesh=mesh,thresholds=[1,.25,.08])
    assert not call(S,'is_nanite_enabled',mesh=mesh)
    slots=call(S,'get_material_slots',mesh=mesh);assert len(slots)==1,slots
    assert call(S,'set_material',mesh=mesh,slot_name=slots[0],material=mat)
    call(A,'save_assets',asset_paths=[mesh['refPath']])
    records[name]={'mesh':mesh,'bounds':bounds,'triangles':[call(S,'get_triangle_count',mesh=mesh,lod_index=i) for i in range(3)],'simpleCollision':'authored UCX slab; runtime collision/navigation disabled'}
    (JOB/'evidence/unreal-meshes.json').write_text(json.dumps(records,indent=2));print(name,flush=True)
assert not call(A,'exists',path=ROOT+'/BP_Road_Review')
bp=call(B,'create',folder_path=ROOT,asset_name='BP_Road_Review',asset_type={'refPath':'/Script/Hansa.HansaRoadPresentation'})
cdo=call(B,'get_default_object',blueprint=bp)
props(cdo,{name[0].lower()+name[1:]:records['SM_HansaRoad_'+name]['mesh'] for name in ('Isolated','End','Straight','Corner','TJunction','Crossroads')})
call(B,'compile_blueprint',blueprint=bp);call(A,'save_assets',asset_paths=[bp['refPath']])
(JOB/'evidence/unreal-import.json').write_text(json.dumps({'blueprint':bp,'cdo':cdo,'material':mat,'textures':textures,'meshes':records},indent=2))
print('P18_IMPORTED_TO_STAGING')
