import unreal,toolset_registry,json
from pathlib import Path
from toolset_registry.registration import Registration
P=Path(__file__).resolve().parents[1];ROOT='/Game/Hansa/Generated/Staging/HansaDirtRoad_20260906_01';LEVEL='/Game/Hansa/Developer/GenerationPreview/HansaDirtRoad_20260906_01/L_DirtRoad_Review'
def guard():
 assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()==(P.parents[2]/'Hansa.uproject').resolve()
 assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().startswith(LEVEL+'.')
@unreal.uclass()
class HansaRoadReviewTools(unreal.ToolsetDefinition):
 @toolset_registry.tool_call
 @staticmethod
 def inspect() -> str:
  """Read road preview actor/component transforms, material expressions and editor view capabilities."""
  guard();rows=[]
  for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
   if a.get_actor_label().startswith(('SM_Dirt','DirtRoad')):
    rows.append({'label':a.get_actor_label(),'location':str(a.get_actor_location()),'rotation':str(a.get_actor_rotation()),'components':[{'name':c.get_name(),'location':str(c.get_editor_property('relative_location')),'transform':str(c.get_world_transform())} for c in a.get_components_by_class(unreal.SceneComponent)]})
  m=unreal.load_asset(ROOT+'/Materials/M_DirtRoad');expr=unreal.MaterialEditingLibrary.get_material_expressions(m) if hasattr(unreal.MaterialEditingLibrary,'get_material_expressions') else []
  data={'actors':rows,'expressions':[str(e) for e in expr],'gameview':[n for n in dir(unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)) if 'game_view' in n]}
  (P/'evidence/unreal_review_diagnostic.json').write_text(json.dumps(data,indent=2));return json.dumps(data)
 @toolset_registry.tool_call
 @staticmethod
 def correct_preview() -> str:
  """Correct this road preview's component offsets, orient junction pieces and hide editor helpers; save only the review map and its spline example."""
  guard();sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
  for a in sub.get_all_level_actors():
   label=a.get_actor_label()
   if label=='SM_DirtRoad_Corner90_R6m':a.set_actor_location(unreal.Vector(700,-800,0),False,False);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-90,roll=0),False)
   if label=='SM_DirtRoad_TJunction_12m':a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=180,roll=0),False)
   if label.startswith('DirtRoad'):
    for c in a.get_components_by_class(unreal.SplineMeshComponent):c.set_editor_property('relative_location',unreal.Vector());c.set_editor_property('relative_rotation',unreal.Rotator());c.set_editor_property('relative_scale3d',unreal.Vector(1,1,1));c.update_mesh()
  sub.set_selected_level_actors([])
  le=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
  if hasattr(le,'editor_set_game_view'):le.editor_set_game_view(True)
  unreal.EditorLoadingAndSavingUtils.save_current_level();return 'Saved corrected road preview'
_review_registration=Registration([HansaRoadReviewTools]);_review_registration.register()
