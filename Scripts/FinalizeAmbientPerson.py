"""Assign source PBR maps, LODs and stable clip names to the supplied character."""
import unreal as u, json
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve()
base='/Game/Hansa/Characters/Laborer01'
names={'dig':'Dig','idle':'Idle','look_around':'LookAround','shovel':'Shovel','standing_relax':'StandingRelax','walk':'Walk','wave_goodbye_02':'WaveGoodbye'}
for source,name in names.items():
    old=base+'/SK_Laborer01'+source
    new=base+'/A_Laborer01_'+name
    if not u.EditorAssetLibrary.does_asset_exist(new) and u.EditorAssetLibrary.does_asset_exist(old):
        assert u.EditorAssetLibrary.rename_asset(old,new)
    clip=u.load_asset(new)
    assert clip
    clip.set_editor_property('force_root_lock',True)
    clip.set_editor_property('root_motion_root_lock',u.RootMotionRootLock.ANIM_FIRST_FRAME)
    u.EditorAssetLibrary.save_loaded_asset(clip)
material=u.load_asset(base+'/M_Laborer01')
if not material:
    material=u.AssetToolsHelpers.get_asset_tools().create_asset('M_Laborer01',base,u.Material,u.MaterialFactoryNew())
    maps=root/'Content/Characters/Labor/Labor man 1b/tripo_convert_545e3800-42c3-4467-973c-86872a265c68.fbm'
    for index,(key,suffix,prop) in enumerate([
        ('BaseColor','basecolor.JPEG',u.MaterialProperty.MP_BASE_COLOR),
        ('Normal','normal.PNG',u.MaterialProperty.MP_NORMAL),
        ('Roughness','roughness.JPEG',u.MaterialProperty.MP_ROUGHNESS),
        ('Metallic','metallic.JPEG',u.MaterialProperty.MP_METALLIC)]):
        task=u.AssetImportTask(); task.filename=str(maps/('medieval_day_laborer_3d_model_'+suffix)); task.destination_path=base; task.destination_name='T_Laborer01_'+key; task.automated=True; task.save=True
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture=u.load_asset(task.imported_object_paths[0]); texture.set_editor_property('srgb',key=='BaseColor')
        if key=='Normal':
            texture.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property('flip_green_channel',True)
        elif key!='BaseColor': texture.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
        node=u.MaterialEditingLibrary.create_material_expression(material,u.MaterialExpressionTextureSample,-400,index*200)
        node.set_editor_property('texture',texture)
        node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL if key=='Normal' else (u.MaterialSamplerType.SAMPLERTYPE_COLOR if key=='BaseColor' else u.MaterialSamplerType.SAMPLERTYPE_MASKS))
        assert u.MaterialEditingLibrary.connect_material_property(node,'RGB' if key in ['BaseColor','Normal'] else 'R',prop)
        u.EditorAssetLibrary.save_loaded_asset(texture)
    u.MaterialEditingLibrary.recompile_material(material)
    u.EditorAssetLibrary.save_loaded_asset(material)
material.set_editor_property('used_with_skeletal_mesh',True)
u.MaterialEditingLibrary.recompile_material(material)
u.EditorAssetLibrary.save_loaded_asset(material)
mesh=u.load_asset(base+'/SK_Laborer01')
materials=mesh.get_editor_property('materials')
for index,slot in enumerate(materials):
    slot.set_editor_property('material_interface',material); materials[index]=slot
mesh.set_editor_property('materials',materials)
subsystem=u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
settings=u.load_asset(base+'/LOD_Laborer01')
if not settings:
    factory=u.DataAssetFactory(); factory.set_editor_property('data_asset_class',u.SkeletalMeshLODSettings)
    settings=u.AssetToolsHelpers.get_asset_tools().create_asset('LOD_Laborer01',base,u.SkeletalMeshLODSettings,factory)
groups=[]
for fraction,screen in [(1.0,1.0),(.5,.18),(.2,.06)]:
    group=u.SkeletalMeshLODGroupSettings(); reduction=group.get_editor_property('reduction_settings')
    reduction.set_editor_property('num_of_triangles_percentage',fraction)
    group.set_editor_property('reduction_settings',reduction); group.set_editor_property('screen_size',u.PerPlatformFloat(screen)); group.set_editor_property('lod_hysteresis',.01); groups.append(group)
settings.set_editor_property('lod_groups',groups)
mesh.set_editor_property('lod_settings',settings)
assert subsystem.regenerate_lod(mesh,3,False)
u.EditorAssetLibrary.save_loaded_asset(settings)
u.EditorAssetLibrary.save_loaded_asset(mesh)
registry=u.AssetRegistryHelpers.get_asset_registry(); registry.search_all_assets(True)
options=u.AssetRegistryDependencyOptions(); options.include_hard_package_references=True; options.include_soft_package_references=True
report={}
for path in u.EditorAssetLibrary.list_assets(base,True,False):
    asset=u.load_asset(path)
    if asset.get_class().get_name()=='ObjectRedirector': continue
    deps=[str(p) for p in (registry.get_dependencies(path.split('.')[0],options) or [])]
    assert not any('/Staging/' in d for d in deps)
    report[path]={'class':asset.get_class().get_name(),'dependencies':deps}
    if isinstance(asset,u.AnimSequence): report[path]['seconds']=asset.get_editor_property('sequence_length')
    if isinstance(asset,u.SkeletalMesh): report[path]['lod_vertices']=[subsystem.get_num_verts(asset,i) for i in range(subsystem.get_lod_count(asset))]
(root/'Docs/Development/AmbientPeople/asset-manifest.json').write_text(json.dumps(report,indent=2))
u.log('AMBIENT_PERSON_FINALIZED')
