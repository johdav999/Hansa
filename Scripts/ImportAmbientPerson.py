"""Import the user-supplied rig and all takes, preserving the original source."""
import unreal as u, json, hashlib
from pathlib import Path
root = Path(u.Paths.project_dir()).resolve()
source = root/'Content/Characters/Labor/Labor man 1b/laborman 1.fbx'
base = '/Game/Hansa/Characters/Laborer01'
assert not u.EditorAssetLibrary.does_asset_exist(base+'/SK_Laborer01'), 'Refuse accidental reimport'
ui = u.FbxImportUI()
ui.set_editor_property('import_mesh', True)
ui.set_editor_property('import_as_skeletal', True)
ui.set_editor_property('mesh_type_to_import', u.FBXImportType.FBXIT_SKELETAL_MESH)
ui.set_editor_property('original_import_type', u.FBXImportType.FBXIT_SKELETAL_MESH)
ui.set_editor_property('automated_import_should_detect_type', False)
ui.set_editor_property('import_animations', True)
ui.set_editor_property('import_materials', False)
ui.set_editor_property('import_textures', False)
ui.set_editor_property('create_physics_asset', False)
# Source is approximately one metre tall. Import at an adult 1.8m height;
# the exact imported bounds and animation timings are recorded below.
ui.skeletal_mesh_import_data.set_editor_property('import_uniform_scale', 1.8)
ui.anim_sequence_import_data.set_editor_property('import_uniform_scale', 1.8)
task = u.AssetImportTask()
task.filename = str(source)
task.destination_path = base
task.destination_name = 'SK_Laborer01'
task.automated = True
task.save = True
task.options = ui
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
assets = []
for path in u.EditorAssetLibrary.list_assets(base, True, False):
    a = u.load_asset(path)
    info = {'path': path, 'class': a.get_class().get_name()}
    if isinstance(a, u.AnimSequence):
        a.set_editor_property('force_root_lock', True)
        info['seconds'] = a.get_editor_property('sequence_length')
        info['skeleton'] = a.get_editor_property('skeleton').get_path_name()
        u.EditorAssetLibrary.save_loaded_asset(a)
    if isinstance(a, u.SkeletalMesh):
        info['bounds'] = str(a.get_bounds())
        info['skeleton'] = a.get_editor_property('skeleton').get_path_name()
    assets.append(info)
out = root/'Docs/Development/AmbientPeople'
out.mkdir(parents=True, exist_ok=True)
(out/'unreal-import.json').write_text(json.dumps({'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'assets': assets}, indent=2))
u.log('AMBIENT_PERSON_IMPORTED '+json.dumps(assets))
