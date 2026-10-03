"""User-authorized dog import and promotion; staged dependency-closure validation first."""
import unreal as u,json,hashlib
from pathlib import Path
ROOT=Path(u.Paths.project_dir()).resolve();SOURCE=ROOT/'SourceArt/Generated/Animals/DogMaterial_20260923';OUT=SOURCE/'unreal-integration';STAGE='/Game/Hansa/Generated/Staging/dog-city_20260923_01';DEST='/Game/Hansa/Animals/Dog'
lib=u.EditorAssetLibrary;tools=u.AssetToolsHelpers.get_asset_tools()
def main():
 assert ROOT.name=='Hansa' and (ROOT/'Hansa.uproject').exists()
 assert not lib.does_directory_exist(DEST),'Dog production folder exists; inspect instead of overwriting'
 u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
 options=u.FbxImportUI();options.automated_import_should_detect_type=False;options.import_mesh=True;options.import_as_skeletal=True;options.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH;options.original_import_type=u.FBXImportType.FBXIT_SKELETAL_MESH;options.import_materials=False;options.import_textures=False;options.import_animations=True;options.create_physics_asset=False
 options.skeletal_mesh_import_data.normal_import_method=u.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS;options.skeletal_mesh_import_data.normal_generation_method=u.FBXNormalGenerationMethod.MIKK_T_SPACE
 task=u.AssetImportTask();task.filename=str(OUT/'Dog_Walk_UE.fbx');task.destination_path=STAGE;task.destination_name='SK_Dog';task.automated=True;task.replace_existing=True;task.save=False;task.options=options;task.factory=u.FbxFactory();tools.import_asset_tasks([task])
 objects=[lib.load_asset(p) for p in task.imported_object_paths];mesh=next(o for o in objects if isinstance(o,u.SkeletalMesh));skeleton=mesh.get_editor_property('skeleton');walk=u.load_object(None,STAGE+'/SK_Dog_Anim.SK_Dog_Anim');assert isinstance(walk,u.AnimSequence)
 walk.set_editor_property('enable_root_motion',False);walk.set_editor_property('force_root_lock',True);walk.set_editor_property('root_motion_root_lock',u.RootMotionRootLock.ANIM_FIRST_FRAME);walk.set_editor_property('loop',True)
 assert abs(walk.get_editor_property('sequence_length')-1.2)<.001
 textures={}
 for role,file in [('BaseColor','dog_fur_basecolor_v1.png'),('Normal','dog_fur_normal.png'),('Roughness','dog_fur_roughness.png')]:
  task=u.AssetImportTask();task.filename=str(SOURCE/'r01'/file);task.destination_path=STAGE;task.destination_name='T_Dog_'+role;task.automated=True;task.replace_existing=True;task.save=False;tools.import_asset_tasks([task]);tex=lib.load_asset(task.imported_object_paths[0]);tex.set_editor_property('srgb',role=='BaseColor')
  if role=='Normal':tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP);tex.set_editor_property('flip_green_channel',True)
  elif role=='Roughness':tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
  textures[role]=tex
 mat=tools.create_asset('M_Dog',STAGE,u.Material,u.MaterialFactoryNew());mat.set_editor_property('used_with_skeletal_mesh',True)
 for role,prop in [('BaseColor',u.MaterialProperty.MP_BASE_COLOR),('Normal',u.MaterialProperty.MP_NORMAL),('Roughness',u.MaterialProperty.MP_ROUGHNESS)]:
  node=u.MaterialEditingLibrary.create_material_expression(mat,u.MaterialExpressionTextureSample);node.texture=textures[role];node.sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL if role=='Normal' else (u.MaterialSamplerType.SAMPLERTYPE_MASKS if role=='Roughness' else u.MaterialSamplerType.SAMPLERTYPE_COLOR);u.MaterialEditingLibrary.connect_material_property(node,'R' if role=='Roughness' else 'RGB',prop)
 for value,prop in [(0.,u.MaterialProperty.MP_METALLIC),(.28,u.MaterialProperty.MP_SPECULAR)]:
  node=u.MaterialEditingLibrary.create_material_expression(mat,u.MaterialExpressionConstant);node.r=value;u.MaterialEditingLibrary.connect_material_property(node,'',prop)
 u.MaterialEditingLibrary.recompile_material(mat)
 slots=mesh.get_editor_property('materials');slot=slots[0];slot.set_editor_property('material_interface',mat);slots[0]=slot;mesh.set_editor_property('materials',slots)
 # 19k source triangles already fit the bounded three-dog budget. Preserve LOD0.
 subsystem=u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
 selected=[(skeleton,'SKEL_Dog'),(mesh,'SK_Dog'),(walk,'A_Dog_Walk'),(mat,'M_Dog')]+[(t,'T_Dog_'+role) for role,t in textures.items()]
 for obj,_ in selected:assert lib.save_loaded_asset(obj,False)
 assert tools.rename_assets([u.AssetRenameData(obj,DEST,name) for obj,name in selected])
 for obj,_ in selected:assert lib.save_loaded_asset(obj,False)
 registry=u.AssetRegistryHelpers.get_asset_registry();deps={}
 for obj,_ in selected:
  path=obj.get_path_name().split('.')[0];deps[path]=[str(x) for x in (registry.get_dependencies(path,u.AssetRegistryDependencyOptions(True,True,False,False,False)) or [])];assert not any('/Generated/Staging/' in x for x in deps[path]),deps[path]
 report={'approval':'User: Import this to unreal engine and use it in the game; add 2-3 dogs walking around in town','status':'promoted-user-authorized','engine':u.SystemLibrary.get_engine_version(),'source_sha256':hashlib.sha256((OUT/'Dog_Walk_UE.fbx').read_bytes()).hexdigest(),'assets':[o.get_path_name() for o,_ in selected],'duration':walk.get_editor_property('sequence_length'),'vertices':subsystem.get_num_verts(mesh,0),'dependencies':deps}
 (OUT/'promotion.json').write_text(json.dumps(report,indent=2));u.log('DOG_PROMOTION_COMPLETE')
try:main()
except Exception as e:(OUT/'import_failure.json').write_text(json.dumps({'error':str(e)}));raise
