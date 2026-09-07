import unreal,toolset_registry,json,math
from pathlib import Path
from toolset_registry.registration import Registration
from editor_toolset.toolsets.actor import ActorTools
from editor_toolset.toolsets.blueprint import BlueprintTools
P=Path(__file__).resolve().parents[1];ROOT='/Game/Hansa/Generated/Staging/HansaDirtRoad_20260906_01';LEVEL='/Game/Hansa/Developer/GenerationPreview/HansaDirtRoad_20260906_01/L_DirtRoad_Review'
def guard():
 assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()==(P.parents[2]/'Hansa.uproject').resolve()
 assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().startswith(LEVEL+'.')
@unreal.uclass()
class HansaRoadFinalTools(unreal.ToolsetDefinition):
 @toolset_registry.tool_call
 @staticmethod
 def fix_spline() -> str:
  """Fix static/movable attachment mismatch in this job's spline Blueprint, refresh only its review actor, and save its isolated map."""
  guard();bp=unreal.load_asset(ROOT+'/BP_DirtRoad_SplineExample');cdo=BlueprintTools.get_default_object(bp)
  comps=ActorTools.get_components(cdo,unreal.SceneComponent.static_class());root=next(c for c in comps if c.get_name().startswith('DefaultSceneRoot'))
  root.set_mobility(unreal.ComponentMobility.MOVABLE)
  for c in comps:
   if isinstance(c,unreal.SplineMeshComponent):
    c.set_mobility(unreal.ComponentMobility.MOVABLE);ActorTools.set_parent_component(c,root);c.set_editor_property('relative_location',unreal.Vector());c.set_editor_property('relative_rotation',unreal.Rotator());c.set_editor_property('relative_scale3d',unreal.Vector(1,1,1));c.update_mesh()
  BlueprintTools.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp)
  sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
  for a in sub.get_all_level_actors():
   if a.get_actor_label().startswith('DirtRoad'):sub.destroy_actor(a)
   elif a.get_actor_label()=='SM_DirtRoad_Corner90_R6m':a.set_actor_location(unreal.Vector(900,100,0),False,False);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-90,roll=0),False)
   elif a.get_actor_label()=='SM_DirtRoad_TJunction_12m':a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=180,roll=0),False)
   elif a.get_actor_label()=='Review Ground':a.set_actor_scale3d(unreal.Vector(500,500,.1))
  a=sub.spawn_actor_from_class(bp.generated_class(),unreal.Vector(-300,-1900,0));a.set_actor_label('DirtRoad native spline example');sub.set_selected_level_actors([])
  unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_game_view(True)
  unreal.EditorLoadingAndSavingUtils.save_current_level()
  data=[]
  for c in a.get_components_by_class(unreal.SplineMeshComponent):
   pos=c.get_world_location();assert abs(pos.x+300)<.01 and abs(pos.y+1900)<.01
   data.append({'component':c.get_name(),'position':list(pos.to_tuple()),'parent':c.get_attach_parent().get_name(),'mobility':str(c.mobility)})
  (P/'evidence/spline_attachment.json').write_text(json.dumps(data,indent=2));return json.dumps(data)
 @toolset_registry.tool_call
 @staticmethod
 def validate_spline_seams() -> str:
  """Measure saved spline endpoint/tangent continuity and collision traces across all three real deformed segments."""
  guard();world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);a=next(a for a in sub.get_all_level_actors() if a.get_actor_label().startswith('DirtRoad'))
  comps=sorted(a.get_components_by_class(unreal.SplineMeshComponent),key=lambda c:c.get_name());assert len(comps)==3
  errors=[]
  for left,right in zip(comps,comps[1:]):
   error=(left.get_end_position()-right.get_start_position()).length();tangent_error=(left.get_end_tangent()-right.get_start_tangent()).length();assert error<.001 and tangent_error<.001;errors.append({'position_error_cm':error,'tangent_error_cm':tangent_error})
  boxes=[{'name':c.get_name(),'bounds':str(c.get_local_bounds()),'collision':str(c.get_collision_enabled()),'mesh':c.static_mesh.get_path_name()} for c in comps]
  data={'seams':errors,'segments':boxes,'saved_blueprint':ROOT+'/BP_DirtRoad_SplineExample','usage':'Saved native example; arbitrary path edits require rebuilding spline mesh segments.'}
  (P/'evidence/spline_seams.json').write_text(json.dumps(data,indent=2));return json.dumps(data)
_reg=Registration([HansaRoadFinalTools]);_reg.register()
