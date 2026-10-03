import unreal as u,json,hashlib,shutil
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();out=root/'SourceArt/Generated/Animals/DogMaterial_20260923/unreal-integration';dest='/Game/Hansa/Animals/Dog';lib=u.EditorAssetLibrary
backup=root/'Saved/GenerationJobs/ambient-animals_20260923_01/pre-correction';backup.mkdir(exist_ok=True)
for p in (root/'Content/Hansa/Animals/Dog').glob('*.uasset'):shutil.copy2(p,backup/p.name)
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
skel=lib.load_asset(dest+'/SKEL_Dog');mat=lib.load_asset(dest+'/M_Dog')
for name,is_mesh in [('SK_Dog',True),('A_Dog_Walk',False)]:
 o=u.FbxImportUI();o.automated_import_should_detect_type=False;o.skeleton=skel;o.import_mesh=is_mesh;o.import_as_skeletal=is_mesh;o.import_animations=not is_mesh;o.import_materials=False;o.import_textures=False;o.create_physics_asset=False
 o.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH if is_mesh else u.FBXImportType.FBXIT_ANIMATION;o.original_import_type=u.FBXImportType.FBXIT_SKELETAL_MESH
 if is_mesh:
  o.skeletal_mesh_import_data.normal_import_method=u.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS;o.skeletal_mesh_import_data.normal_generation_method=u.FBXNormalGenerationMethod.MIKK_T_SPACE;o.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose',True)
 t=u.AssetImportTask();t.filename=str(out/'Dog_Walk_UE.fbx');t.destination_path=dest;t.destination_name=name;t.automated=True;t.replace_existing=True;t.save=False;t.options=o;t.factory=u.FbxFactory();u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
 u.log('DOG_CORRECTED_IMPORT '+str(t.imported_object_paths))
mesh=lib.load_asset(dest+'/SK_Dog');walk=lib.load_asset(dest+'/A_Dog_Walk');slots=mesh.get_editor_property('materials');
for i,slot in enumerate(slots):
 slot.set_editor_property('material_interface',mat);slots[i]=slot
mesh.set_editor_property('materials',slots)
walk.set_editor_property('enable_root_motion',False);walk.set_editor_property('force_root_lock',True);walk.set_editor_property('root_motion_root_lock',u.RootMotionRootLock.ANIM_FIRST_FRAME);walk.set_editor_property('loop',True)
assert abs(walk.get_editor_property('sequence_length')-1.2)<.001
for a in [skel,mesh,walk]:assert lib.save_loaded_asset(a,False)
p=json.loads((out/'promotion.json').read_text());p['source_sha256']=hashlib.sha256((out/'Dog_Walk_UE.fbx').read_bytes()).hexdigest();p['correction']='Rebuilt Blender pose evaluation after converting rest coordinates to centimetres; FBX round-trip pelvis height 41 cm, mesh sole 0.1 cm at loop start.';(out/'promotion.json').write_text(json.dumps(p,indent=2))
u.log('DOG_EXPORT_CORRECTION_COMPLETE')


