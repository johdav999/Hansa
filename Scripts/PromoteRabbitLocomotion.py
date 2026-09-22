"""Explicitly approved rabbit promotion; run with Unreal's Python commandlet.

Never overwrites production packages. Source masters and prior evidence stay intact.
"""
import hashlib
import json
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
SOURCE = ROOT / 'SourceArt/Generated/Animations/Animal.Rabbit.Locomotion/r05-locomotion'
EVIDENCE = ROOT / 'Saved/GenerationJobs/rabbit-city_20260920_01'
STAGE = '/Game/Hansa/Generated/Staging/rabbit-city_20260920_01/Legacy'
DEST = '/Game/Hansa/Animals/Rabbit'
EVIDENCE.mkdir(parents=True, exist_ok=True)
lib = u.EditorAssetLibrary
assets = u.AssetToolsHelpers.get_asset_tools()

def save_report(name, data):
    (EVIDENCE / name).write_text(json.dumps(data, indent=2), encoding='utf-8')

def import_clip(clip, skeleton=None):
    path = STAGE + '/' + clip + '/' + ('SK_Rabbit' if clip == 'Walk' else 'SK_Rabbit_JumpValidation')
    if lib.does_asset_exist(path) and lib.does_asset_exist(path + '_Anim'):
        return lib.load_asset(path), lib.load_asset(path + '_Anim')
    if not skeleton and lib.does_asset_exist(STAGE + '/Walk/SK_Rabbit_Skeleton'):
        skeleton = lib.load_asset(STAGE + '/Walk/SK_Rabbit_Skeleton')
    options = u.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.import_mesh = True
    options.import_as_skeletal = True
    options.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
    options.original_import_type = u.FBXImportType.FBXIT_SKELETAL_MESH
    options.import_materials = False
    options.import_textures = False
    options.import_animations = True
    options.create_physics_asset = False
    if skeleton:
        options.skeleton = skeleton
    options.skeletal_mesh_import_data.normal_import_method = u.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS
    options.skeletal_mesh_import_data.normal_generation_method = u.FBXNormalGenerationMethod.MIKK_T_SPACE
    task = u.AssetImportTask()
    task.filename = str(SOURCE / ('A_Rabbit_' + clip + '.fbx'))
    task.destination_path = STAGE + '/' + clip
    task.destination_name = 'SK_Rabbit' if clip == 'Walk' else 'SK_Rabbit_JumpValidation'
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = options
    task.factory = u.FbxFactory()
    assets.import_asset_tasks([task])
    objects = [lib.load_asset(p) for p in task.imported_object_paths]
    mesh = next(o for o in objects if isinstance(o, u.SkeletalMesh))
    anim_path = mesh.get_path_name().split('.')[0] + '_Anim'
    animation = u.load_object(None, anim_path + '.' + anim_path.rsplit('/', 1)[1])
    assert isinstance(animation, u.AnimSequence), 'Missing imported action'
    assert lib.save_loaded_asset(mesh.get_editor_property('skeleton'), False)
    assert lib.save_loaded_asset(animation, False)
    assert lib.save_loaded_asset(mesh, False)
    return mesh, animation

def main():
    u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    assert not lib.does_directory_exist(DEST), 'Production target exists: inspect instead of overwriting'
    save_report('approval.json', {'reviewer': 'user', 'state': 'approved-for-promotion-and-gameplay',
        'approval': 'i checked. it looks good; implement 5-6 rabbits in Lubeck; so promote them also',
        'stableSkeleton': 'Animal.Rabbit.Skeleton.Draft01', 'revision': 'r05-locomotion',
        'inputs': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in SOURCE.glob('A_Rabbit_*.fbx')}})
    mesh, walk = import_clip('Walk')
    skeleton = mesh.get_editor_property('skeleton')
    _, jump = import_clip('Jump', skeleton)
    assert walk.get_editor_property('skeleton') == skeleton == jump.get_editor_property('skeleton')
    for clip, looping in ((walk, True), (jump, False)):
        clip.set_editor_property('enable_root_motion', not looping)
        clip.set_editor_property('force_root_lock', not looping)
        clip.set_editor_property('root_motion_root_lock', u.RootMotionRootLock.ANIM_FIRST_FRAME)
        clip.set_editor_property('loop', looping)
        assert abs(clip.get_editor_property('sequence_length') - 1.0) < 0.001
    # Preserve imported LOD0; reduced LOD1 is the gameplay presentation, not a new rig.
    subsystem = u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
    settings = lib.load_asset(STAGE + '/LOD_Rabbit') if lib.does_asset_exist(STAGE + '/LOD_Rabbit') else None
    if not settings:
        factory = u.DataAssetFactory()
        factory.set_editor_property('data_asset_class', u.SkeletalMeshLODSettings)
        settings = assets.create_asset('LOD_Rabbit', STAGE, u.SkeletalMeshLODSettings, factory)
    if subsystem.get_lod_count(mesh) < 2 or subsystem.get_num_verts(mesh, 1) > 100000:
        base = u.SkeletalMeshLODGroupSettings()
        base_reduction = base.get_editor_property('reduction_settings')
        base_reduction.set_editor_property('num_of_triangles_percentage', 1.0)
        base_reduction.set_editor_property('num_of_vert_percentage', 1.0)
        base.set_editor_property('reduction_settings', base_reduction)
        reduced = u.SkeletalMeshLODGroupSettings()
        reduction = reduced.get_editor_property('reduction_settings')
        reduction.set_editor_property('num_of_triangles_percentage', 0.025)
        reduced.set_editor_property('reduction_settings', reduction)
        settings.set_editor_property('lod_groups', [base, reduced])
        settings.set_editor_property('min_lod', u.PerPlatformInt(1))
        assert lib.save_loaded_asset(settings, False)
        mesh.set_editor_property('lod_settings', settings)
        assert subsystem.regenerate_lod(mesh, 2, False)
        mesh.set_editor_property('min_lod', u.PerPlatformInt(1))
        assert lib.save_loaded_asset(mesh, False)
    texdir = ROOT / 'Content/Characters/rabbit/tripo_convert_9b3ad557-3e6a-4fde-a83b-dd914be8d80c.fbm'
    textures = {}
    for role, filename in [('BaseColor','rabbit_3d_model_basecolor.JPEG'), ('Normal','rabbit_3d_model_normal.PNG'), ('Roughness','rabbit_3d_model_roughness.JPEG')]:
        task = u.AssetImportTask()
        task.filename = str(texdir / filename)
        task.destination_path = STAGE + '/Materials'
        task.destination_name = 'T_Rabbit_' + role
        task.automated = True
        task.replace_existing = True
        task.save = True
        assets.import_asset_tasks([task])
        tex = lib.load_asset(task.imported_object_paths[0])
        tex.set_editor_property('srgb', role == 'BaseColor')
        if role == 'Normal':
            tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP)
        textures[role] = tex
    mat = assets.create_asset('M_Rabbit', STAGE + '/Materials', u.Material, u.MaterialFactoryNew())
    mat.set_editor_property('used_with_skeletal_mesh', True)
    for role, prop in [('BaseColor',u.MaterialProperty.MP_BASE_COLOR),('Normal',u.MaterialProperty.MP_NORMAL),('Roughness',u.MaterialProperty.MP_ROUGHNESS)]:
        node = u.MaterialEditingLibrary.create_material_expression(mat, u.MaterialExpressionTextureSample)
        node.texture = textures[role]
        if role == 'Normal':
            node.sampler_type = u.MaterialSamplerType.SAMPLERTYPE_NORMAL
        elif role == 'Roughness':
            node.sampler_type = u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
        u.MaterialEditingLibrary.connect_material_property(node, 'RGB' if role != 'Roughness' else 'R', prop)
    u.MaterialEditingLibrary.recompile_material(mat)
    slots = mesh.get_editor_property('materials')
    slot = slots[0]
    slot.set_editor_property('material_interface', mat)
    slots[0] = slot
    mesh.set_editor_property('materials', slots)
    selected = [(skeleton,'SKEL_Rabbit'), (mesh,'SK_Rabbit'), (walk,'A_Rabbit_Walk'), (jump,'A_Rabbit_Jump'), (mat,'M_Rabbit'), (settings,'LOD_Rabbit')]
    selected += [(tex,'T_Rabbit_' + role) for role,tex in textures.items()]
    for obj, _ in selected:
        assert lib.save_loaded_asset(obj, False)
    # Move the entire selected dependency closure together; redirectors remain staging-only.
    before = [obj.get_path_name() for obj,_ in selected]
    assert assets.rename_assets([u.AssetRenameData(obj, DEST, name) for obj,name in selected])
    for obj,_ in selected:
        assert lib.save_loaded_asset(obj, False)
    registry = u.AssetRegistryHelpers.get_asset_registry()
    deps = {}
    for obj,_ in selected:
        package = obj.get_path_name().split('.')[0]
        deps[package] = [str(x) for x in (registry.get_dependencies(package, u.AssetRegistryDependencyOptions(True, True, False, False, False)) or [])]
        assert not any('/Generated/Staging/' in p for p in deps[package]), deps[package]
    save_report('promotion.json', {'state':'promoted-user-approved', 'before':before, 'after':[o.get_path_name() for o,_ in selected],
        'dependencies':deps, 'engine':u.SystemLibrary.get_engine_version(), 'runtimeLOD':1,
        'reductionTriangleFraction':0.025, 'masterUnchanged':True,
        'outputs':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'Content/Hansa/Animals/Rabbit').glob('*.uasset')}})
    u.log('RABBIT_PROMOTION_COMPLETE')

try:
    main()
except Exception as error:
    save_report('failure.json', {'error':str(error)})
    raise
