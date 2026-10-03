"""Bounded local MCP orchestration for the four bakery props."""
import sys, json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'SourceArt/Generated/Trees/LubeckSummer/v1/scripts'))
from ue_batch import call, invoke
OUT=ROOT/'SourceArt/Generated/Props/BakeryTripo_20260922'
OUT.mkdir(parents=True,exist_ok=True)
AS='editor_toolset.toolsets.asset.AssetTools'
SM='editor_toolset.toolsets.static_mesh.StaticMeshTools'
TX='editor_toolset.toolsets.texture.TextureTools'
MT='editor_toolset.toolsets.material.MaterialTools'
OB='editor_toolset.toolsets.object.ObjectTools'
BASE='/Game/Mesh/hansa-bakery/Props'
def ref(path): return {'refPath':path}
def properties(obj, values):
    assert call(OB,'set_properties',instance=obj,values=json.dumps(values)), (obj,values)
if sys.argv[1]=='discover':
    for group in ['HansaEditor.HansaCityTerrainToolset','EditorToolset.EditorAppToolset']+['editor_toolset.toolsets.'+x for x in ['asset.AssetTools','object.ObjectTools','static_mesh.StaticMeshTools','texture.TextureTools','material.MaterialTools','scene.SceneTools','actor.ActorTools']]:
        info=invoke('describe_toolset',{'toolset_name':group})
        if isinstance(info,str): info=json.loads(info)
        (OUT/(group.split('.')[-1]+'-schema.json')).write_text(json.dumps(info,indent=2))
        print(group)
        for t in info['tools']:
            if t['name'].split('.')[-1] in ['InspectAuthoringContext','get_properties','set_properties','list_properties','import_file','get_bounds','create_material','add_expression','connect_to_output','set_material','generate_lods','generate_convex_collisions','save_assets','SetCameraTransform','CaptureScreenshot','TakeScreenshot','GetEditorState','add_to_scene_from_asset','add_to_scene_from_class','duplicate','load_level']:
                print(json.dumps(t))
elif sys.argv[1]=='context':
    print(call('HansaEditor.HansaCityTerrainToolset','InspectAuthoringContext'))
elif sys.argv[1]=='import':
    state=call('HansaEditor.HansaCityTerrainToolset','InspectAuthoringContext')
    if isinstance(state,str):state=json.loads(state)
    assert not state.get('pieRunning',True)
    assert state['projectFile'].replace('\\','/').lower()==str(ROOT/'Hansa.uproject').replace('\\','/').lower()
    records=json.loads((OUT/'manifest.json').read_text())
    evidence=[]
    for item in records:
        name=item['name']
        saved=[]
        matpath=BASE+'/Materials/M_Bakery_'+name
        assert not call(AS,'exists',path=matpath), 'Refuse to overwrite existing material'
        material=call(MT,'create_material',folder_path=BASE+'/Materials',asset_name='M_Bakery_'+name)
        saved.append(matpath)
        for i,(kind,source) in enumerate(item['maps'].items()):
            tex=call(TX,'import_file',folder_path=BASE+'/Textures',asset_name='T_Bakery_'+name+'_'+kind.capitalize(),source_file=str(ROOT/source))[0]
            saved.append(tex['refPath'])
            values={'sRGB':kind=='basecolor'}
            if kind=='normal': values.update(compressionSettings='TC_Normalmap',bFlipGreenChannel=True)
            elif kind in ['roughness','metallic']:values['compressionSettings']='TC_Masks'
            properties(tex,values)
            node=call(MT,'add_expression',material_or_function=material,expression_class=ref('/Script/Engine.MaterialExpressionTextureSample'),x=-500,y=i*240)
            properties(node,{'texture':tex['refPath'],'samplerType':'SAMPLERTYPE_Normal' if kind=='normal' else ('SAMPLERTYPE_Color' if kind=='basecolor' else 'SAMPLERTYPE_Masks')})
            output={'basecolor':'BaseColor','normal':'Normal','roughness':'Roughness','metallic':'Metallic'}[kind]
            call(MT,'connect_to_output',expression=node,output_name='RGB' if kind in ['basecolor','normal'] else 'R',material_property='MP_'+output)
        call(MT,'recompile',material_or_function=material)
        mesh=call(SM,'import_file',folder_path=BASE+'/Meshes',asset_name='SM_Bakery_'+name,source_file=str(OUT/('SM_Bakery_'+name+'.fbx')),import_materials=False,import_textures=False,combine_meshes=True)[0]
        saved.append(mesh['refPath'])
        for slot in call(SM,'get_material_slots',mesh=mesh):
            assert call(SM,'set_material',mesh=mesh,slot_name=slot,material=material)
        assert call(SM,'generate_lods',mesh=mesh,triangle_percents=[0.5,0.2])==3
        assert call(SM,'set_lod_thresholds',mesh=mesh,thresholds=[1.0,0.25,0.08])
        assert call(SM,'generate_convex_collisions',mesh=mesh,hull_count=4,max_hull_verts=16,hull_precision=100000)
        call(SM,'set_nanite_enabled',mesh=mesh,enabled=False)
        assert call(AS,'save_assets',asset_paths=saved)
        record={'name':name,'mesh':mesh,'boundsCm':call(SM,'get_bounds',mesh=mesh),'triangles':[call(SM,'get_triangle_count',mesh=mesh,lod_index=i) for i in range(3)],'assets':saved}
        evidence.append(record)
        (OUT/'unreal-import.json').write_text(json.dumps(evidence,indent=2))
        print(json.dumps(record),flush=True)
elif sys.argv[1]=='preview':
    SC='editor_toolset.toolsets.scene.SceneTools'
    AC='editor_toolset.toolsets.actor.ActorTools'
    APP='EditorToolset.EditorAppToolset'
    # Review in an isolated unsaved Entry session; never save an engine map.
    state=call('HansaEditor.HansaCityTerrainToolset','InspectAuthoringContext')
    if isinstance(state,str):state=json.loads(state)
    assert state['map']=='/Engine/Maps/Entry' and not state['pieRunning']
    level=state['map']
    def pose(x,y,z,pitch=0,yaw=0):return {'location':{'x':x,'y':y,'z':z},'rotation':{'pitch':pitch,'yaw':yaw,'roll':0}}
    actor=call(SC,'add_to_scene_from_class',actor_type=ref('/Script/Hansa.HansaBakeryPresentation'),name='BakeryProductionProps',xform=pose(0,0,0))
    floor=pose(0,0,-8);floor['scale']={'x':24,'y':20,'z':0.1}
    call(SC,'add_to_scene_from_asset',asset_path='/Engine/BasicShapes/Cube',name='ReviewGround',xform=floor)
    call(SC,'add_to_scene_from_class',actor_type=ref('/Script/Engine.DirectionalLight'),name='ReviewSun',xform=pose(0,0,1800,-50,-120))
    call(SC,'add_to_scene_from_class',actor_type=ref('/Script/Engine.SkyLight'),name='ReviewSkyLight',xform=pose(0,0,1600))
    call(SC,'add_to_scene_from_class',actor_type=ref('/Script/Engine.SkyAtmosphere'),name='ReviewAtmosphere',xform=pose(0,0,0))
    components=call(AC,'get_components',actor=actor,component_type=ref('/Script/Engine.StaticMeshComponent'))
    info=[]
    for c in components:
        info.append({'component':c,'properties':call(OB,'get_properties',instance=c,properties=['staticMesh','relativeLocation','relativeScale3D','bVisible'])})
    (OUT/'native-components.json').write_text(json.dumps(info,indent=2))
    call(APP,'SetCameraTransform',transform=pose(1550,1850,1150,-25,-130))
    print(json.dumps({'level':level,'actor':actor,'components':len(components)}))
elif sys.argv[1]=='capture':
    import base64
    APP='EditorToolset.EditorAppToolset'
    side=sys.argv[2] if len(sys.argv)>2 else 'front'
    locations={'front':(1550,1850,1150,-25,-130),'west':(-1550,1250,700,-17,-38),'east':(1450,450,550,-15,-160)}
    x,y,z,pitch,yaw=locations[side]
    transform={'location':{'x':x,'y':y,'z':z},'rotation':{'pitch':pitch,'yaw':yaw,'roll':0}}
    result=call(APP,'CaptureViewport',captureTransform=transform,annotations=None,bShowUI=False)
    path=OUT/('Unreal-bakery-'+side+'.png')
    path.write_bytes(base64.b64decode(result['image']['data']))
    del result['image']
    (OUT/('Unreal-bakery-'+side+'.json')).write_text(json.dumps(result,indent=2))
    print(path)
