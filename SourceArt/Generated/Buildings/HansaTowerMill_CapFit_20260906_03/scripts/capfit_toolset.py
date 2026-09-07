import unreal,toolset_registry,json
from pathlib import Path
from toolset_registry.registration import Registration
from editor_toolset.toolsets.asset import import_asset
P=Path(__file__).resolve().parents[1]
ROOT='/Game/Hansa/Generated/Staging/HansaTowerMill_20260906_02'
@unreal.uclass()
class HansaCapFitTools(unreal.ToolsetDefinition):
 @toolset_registry.tool_call
 @staticmethod
 def identity() -> str:
  """Read editor project and dirty map state before importing the cap correction."""
  return json.dumps({'project':unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path()),'dirty_maps':[o.get_path_name() for o in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]})
 @toolset_registry.tool_call
 @staticmethod
 def import_capfit() -> str:
  """Import only the corrected cap-fit FBX as a new staging mesh, reusing the eight existing weathered materials."""
  assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()==(P.parents[2]/'Hansa.uproject').resolve()
  name='SM_HansaTowerMill_CapFit'
  assert not unreal.EditorAssetLibrary.does_asset_exist(ROOT+'/Meshes/'+name)
  opts=unreal.FbxImportUI();opts.automated_import_should_detect_type=False;opts.import_mesh=True;opts.import_as_skeletal=False;opts.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opts.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH;opts.import_materials=False;opts.import_textures=False;opts.import_animations=False;opts.static_mesh_import_data.combine_meshes=True;opts.static_mesh_import_data.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
  mesh=import_asset(ROOT+'/Meshes',name,str(P/'exports/HansaTowerMill_CapFit.fbx'),options=opts,factory=unreal.FbxFactory())[0]
  for rec in json.loads((P/'unreal_materials.json').read_text()):
   idx=mesh.get_material_index(rec['name']);assert idx>=0;mesh.set_material(idx,unreal.load_asset(rec['material']['refPath']))
  unreal.EditorAssetLibrary.save_loaded_asset(mesh)
  (P/'unreal_mesh.json').write_text(json.dumps({'refPath':mesh.get_path_name()},indent=2))
  return mesh.get_path_name()
_cap_registration=Registration([HansaCapFitTools]);_cap_registration.register()
unreal.log('HANSA_CAPFIT_TOOL_READY')
