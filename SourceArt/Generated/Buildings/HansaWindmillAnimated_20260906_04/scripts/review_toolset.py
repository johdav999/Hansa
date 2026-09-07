import unreal,toolset_registry,json
from pathlib import Path
from toolset_registry.registration import Registration
P=Path(__file__).resolve().parents[1];LEVEL='/Game/Hansa/Developer/GenerationPreview/HansaWindmillAnimated_20260906_04/L_AnimatedWindmill'
@unreal.uclass()
class HansaWindmillReviewTools(unreal.ToolsetDefinition):
 @toolset_registry.tool_call
 @staticmethod
 def prepare_preview() -> str:
  """Set GameModeBase only in the isolated windmill preview so Hansa scenario actors do not spawn during its animation review."""
  world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_path_name().startswith(LEVEL+'.')
  settings=world.get_world_settings();settings.set_editor_property('default_game_mode',unreal.GameModeBase.static_class());unreal.EditorLoadingAndSavingUtils.save_current_level();return settings.get_path_name()
 @toolset_registry.tool_call
 @staticmethod
 def verify_saved() -> str:
  """Read the reopened animated windmill hierarchy, mesh assignments, pivot and native movement defaults."""
  world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_path_name().startswith(LEVEL+'.')
  actors=unreal.GameplayStatics.get_all_actors_with_tag(world,'HansaAnimatedWindmill');assert len(actors)==1;a=actors[0];child=a.get_component_by_class(unreal.ChildActorComponent);r=child.get_editor_property('child_actor');m=r.get_component_by_class(unreal.RotatingMovementComponent)
  def mesh_info(mesh):
   return {'path':mesh.get_path_name(),'materials':[s.material_interface.get_path_name() for s in mesh.static_materials],'bounds_origin':list(mesh.get_bounds().origin.to_tuple()),'bounds_extent':list(mesh.get_bounds().box_extent.to_tuple())}
  result={'level':LEVEL,'body':mesh_info(a.static_mesh_component.static_mesh),'rotor':mesh_info(r.static_mesh_component.static_mesh),'pivot_cm':list(child.relative_location.to_tuple()),'rotation_rate':str(m.rotation_rate),'local_space':m.rotation_in_local_space,'rotor_movable':r.static_mesh_component.mobility==unreal.ComponentMobility.MOVABLE,'auto_activate':m.auto_activate,'game_mode':world.get_world_settings().default_game_mode.get_path_name()}
  assert m.rotation_rate.pitch==36 and m.rotation_rate.yaw==0 and m.rotation_rate.roll==0 and result['local_space'] and result['rotor_movable'] and result['auto_activate'];(P/'unreal_saved_verification.json').write_text(json.dumps(result,indent=2));return json.dumps(result)
_review_registration=Registration([HansaWindmillReviewTools]);_review_registration.register()
