import unreal,toolset_registry,json
from pathlib import Path
from toolset_registry.registration import Registration
P=Path(__file__).resolve().parents[1];ROOT='/Game/Hansa/Generated/Staging/HansaDirtRoad_20260906_01';LEVEL='/Game/Hansa/Developer/GenerationPreview/HansaDirtRoad_20260906_01/L_DirtRoad_Review'
@unreal.uclass()
class HansaRoadMaterialFinishTools(unreal.ToolsetDefinition):
 @toolset_registry.tool_call
 @staticmethod
 def tune_relief() -> str:
  """Reduce aliased micro-normal relief on this road's shared staging material and enable clean review view."""
  assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()==(P.parents[2]/'Hansa.uproject').resolve()
  m=unreal.load_asset(ROOT+'/Materials/M_DirtRoad');ml=unreal.MaterialEditingLibrary
  scales=[e for e in ml.get_material_expressions(m) if isinstance(e,unreal.MaterialExpressionConstant3Vector)];assert len(scales)==1
  scales[0].set_editor_property('constant',unreal.LinearColor(.08,.08,1,1));ml.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
  unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_game_view(True);unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors([])
  return 'Road normal XY scale changed from 0.5 to 0.08 for distant-view stability; native source maps unchanged'
 @toolset_registry.tool_call
 @staticmethod
 def clean_view() -> str:
  """Hide editor helpers in the currently opened isolated road review map."""
  assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().startswith(LEVEL+'.')
  unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_game_view(True);unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors([]);return 'Clean review view'
_reg=Registration([HansaRoadMaterialFinishTools]);_reg.register()
