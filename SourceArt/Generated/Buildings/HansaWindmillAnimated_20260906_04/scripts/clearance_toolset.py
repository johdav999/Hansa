import unreal,toolset_registry,json
from pathlib import Path
from toolset_registry.registration import Registration
from editor_toolset.toolsets.asset import import_asset
from editor_toolset.toolsets.blueprint import BlueprintTools
P=Path(__file__).resolve().parents[1];ROOT='/Game/Hansa/Generated/Staging/HansaWindmillAnimated_20260906_04'
@unreal.uclass()
class HansaWindmillClearanceTools(unreal.ToolsetDefinition):
 @toolset_registry.tool_call
 @staticmethod
 def update_clearance() -> str:
  """Import the verified longer-shaft rotor as a unique staging revision and update only this job's rotor mesh and mounting offset."""
  assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()==(P.parents[2]/'Hansa.uproject').resolve()
  name='SM_Windmill_Rotor_Clearance';assert not unreal.EditorAssetLibrary.does_asset_exist(ROOT+'/Meshes/'+name)
  opts=unreal.FbxImportUI();opts.automated_import_should_detect_type=False;opts.import_mesh=True;opts.import_as_skeletal=False;opts.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opts.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH;opts.import_materials=False;opts.import_textures=False;opts.import_animations=False;opts.static_mesh_import_data.combine_meshes=True;opts.static_mesh_import_data.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE;opts.static_mesh_import_data.auto_generate_collision=False
  mesh=import_asset(ROOT+'/Meshes',name,str(P/'exports/SM_Windmill_Rotor.fbx'),options=opts,factory=unreal.FbxFactory())[0]
  records=json.loads((P/'unreal_materials.json').read_text())
  for slot in ['WeatheredTimber','ForgedIron']:
   rec=next(r for r in records if r['name']==slot);mesh.set_material(mesh.get_material_index(slot),unreal.load_asset(rec['material']['refPath']))
  unreal.EditorAssetLibrary.save_loaded_asset(mesh)
  rotor=unreal.load_asset(ROOT+'/BP_HansaWindmill_Rotor');cdo=BlueprintTools.get_default_object(rotor);cdo.static_mesh_component.set_static_mesh(mesh);BlueprintTools.compile_blueprint(rotor);unreal.EditorAssetLibrary.save_loaded_asset(rotor)
  building=unreal.load_asset(ROOT+'/BP_HansaWindmill_Animated');sds=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);geo=json.loads((P/'geometry.json').read_text());found=False
  for h in sds.k2_gather_subobject_data_for_blueprint(building):
   d=unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h);o=unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(d)
   if isinstance(o,unreal.ChildActorComponent):o.set_editor_property('relative_location',unreal.Vector(*geo['pivot_unreal_cm']));found=True
  assert found;BlueprintTools.compile_blueprint(building);unreal.EditorAssetLibrary.save_loaded_asset(building);unreal.EditorLoadingAndSavingUtils.save_current_level()
  record=json.loads((P/'unreal_animation.json').read_text());record['meshes']['Rotor']={'refPath':mesh.get_path_name()};record['pivot_cm']=geo['pivot_unreal_cm'];record['clearance_correction_m']=.6;(P/'unreal_animation.json').write_text(json.dumps(record,indent=2));return json.dumps(record)
_clearance_registration=Registration([HansaWindmillClearanceTools]);_clearance_registration.register()
