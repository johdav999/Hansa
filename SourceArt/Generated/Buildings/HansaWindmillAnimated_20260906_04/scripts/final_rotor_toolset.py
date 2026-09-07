import unreal,toolset_registry,json
from pathlib import Path
from toolset_registry.registration import Registration
from editor_toolset.toolsets.actor import ActorTools
from editor_toolset.toolsets.blueprint import BlueprintTools
P=Path(__file__).resolve().parents[1];ROOT='/Game/Hansa/Generated/Staging/HansaWindmillAnimated_20260906_04'
@unreal.uclass()
class HansaFinalRotorTools(unreal.ToolsetDefinition):
 @toolset_registry.tool_call
 @staticmethod
 def finish_rotor() -> str:
  """Create a fresh rotor archetype from the corrected shaft mesh and replace the child class only in the windmill draft Blueprint."""
  assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()==(P.parents[2]/'Hansa.uproject').resolve();assert not unreal.EditorAssetLibrary.does_asset_exist(ROOT+'/BP_HansaWindmill_Rotor_Clearance')
  rotor=BlueprintTools.create(ROOT,'BP_HansaWindmill_Rotor_Clearance',unreal.StaticMeshActor.static_class());m=ActorTools.add_component(rotor,unreal.RotatingMovementComponent.static_class(),'SailRotation');m.set_editor_property('rotation_rate',unreal.Rotator(pitch=36,yaw=0,roll=0));m.set_editor_property('rotation_in_local_space',True);m.set_editor_property('auto_activate',True)
  cdo=BlueprintTools.get_default_object(rotor);mesh=unreal.load_asset(ROOT+'/Meshes/SM_Windmill_Rotor_Clearance');cdo.static_mesh_component.set_static_mesh(mesh);cdo.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE);cdo.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);BlueprintTools.compile_blueprint(rotor);unreal.EditorAssetLibrary.save_loaded_asset(rotor)
  building=unreal.load_asset(ROOT+'/BP_HansaWindmill_Animated');sds=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
  for h in sds.k2_gather_subobject_data_for_blueprint(building):
   o=unreal.SubobjectDataBlueprintFunctionLibrary.get_associated_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))
   if isinstance(o,unreal.ChildActorComponent):o.set_child_actor_class(rotor.generated_class())
  BlueprintTools.compile_blueprint(building);unreal.EditorAssetLibrary.save_loaded_asset(building);unreal.EditorLoadingAndSavingUtils.save_current_level();rec=json.loads((P/'unreal_animation.json').read_text());rec['rotor_blueprint']=rotor.get_path_name();(P/'unreal_animation.json').write_text(json.dumps(rec,indent=2));return rotor.get_path_name()
_final_registration=Registration([HansaFinalRotorTools]);_final_registration.register()
