import unreal,toolset_registry,json
from pathlib import Path
from toolset_registry.registration import Registration
from editor_toolset.toolsets.asset import import_asset
P=Path(__file__).resolve().parents[1]
@unreal.uclass()
class HansaMillWeatheringTools(unreal.ToolsetDefinition):
    @toolset_registry.tool_call
    @staticmethod
    def import_weathered_mill() -> str:
        """Import this job's fixed windmill FBX with authored weathering colors into its staging folder only."""
        assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()==(P.parents[2]/'Hansa.uproject').resolve()
        folder='/Game/Hansa/Generated/Staging/HansaMill_20260906_01/Meshes'
        options=unreal.FbxImportUI()
        options.automated_import_should_detect_type=False
        options.import_mesh=True
        options.import_as_skeletal=False
        options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
        options.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH
        options.import_materials=False
        options.import_textures=False
        options.import_animations=False
        options.static_mesh_import_data.combine_meshes=True
        options.static_mesh_import_data.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
        meshes=import_asset(folder,'SM_HansaMill_Weathered',str(P/'exports/HansaMill.fbx'),options=options,factory=unreal.FbxFactory())
        mesh=meshes[0]
        for r in json.loads((P/'unreal_materials.json').read_text()):
            index=mesh.get_material_index(r['name'])
            assert index>=0
            mesh.set_material(index,unreal.load_asset(r['material']['refPath']))
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        result={'refPath':mesh.get_path_name()}
        (P/'unreal_mesh.json').write_text(json.dumps(result))
        return json.dumps(result)
_mill_registration=Registration([HansaMillWeatheringTools])
_mill_registration.register()
unreal.log('HANSA_MILL_WEATHERING_TOOL_REGISTERED')
