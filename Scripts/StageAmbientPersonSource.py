import unreal as u
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve()
base='/Game/Hansa/Generated/Staging/AmbientPeopleSource'
ui=u.FbxImportUI(); ui.import_mesh=True; ui.import_as_skeletal=True; ui.import_animations=True; ui.import_materials=False; ui.import_textures=False; ui.create_physics_asset=False
ui.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH; ui.original_import_type=u.FBXImportType.FBXIT_SKELETAL_MESH; ui.automated_import_should_detect_type=False
ui.skeletal_mesh_import_data.import_uniform_scale=1.8; ui.anim_sequence_import_data.import_uniform_scale=1.8
if not u.EditorAssetLibrary.does_asset_exist(base+'/Source'):
    t=u.AssetImportTask(); t.filename=str(root/'Content/Characters/Labor/Labor man 1b/laborman 1.fbx'); t.destination_path=base; t.destination_name='Source'; t.automated=True; t.save=True; t.options=ui
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
u.log('SOURCE_TAKES_READY '+str(u.EditorAssetLibrary.list_assets(base)))
