import unreal,toolset_registry,json
from pathlib import Path
from toolset_registry.registration import Registration
from editor_toolset.toolsets.asset import import_asset
from editor_toolset.toolsets.actor import ActorTools
from editor_toolset.toolsets.blueprint import BlueprintTools
P=Path(__file__).resolve().parents[1];ROOT='/Game/Hansa/Generated/Staging/HansaWindmillAnimated_20260906_04';LEVEL='/Game/Hansa/Developer/GenerationPreview/HansaWindmillAnimated_20260906_04/L_AnimatedWindmill'
def identity():
 project=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve();assert project==(P.parents[2]/'Hansa.uproject').resolve()
 return str(project)
def mill(world):
 actors=unreal.GameplayStatics.get_all_actors_with_tag(world,'HansaAnimatedWindmill');assert len(actors)==1;return actors[0]
@unreal.uclass()
class HansaWindmillAnimationTools(unreal.ToolsetDefinition):
 @toolset_registry.tool_call
 @staticmethod
 def identity() -> str:
  """Verify the Hansa editor project and dirty maps before this windmill revision."""
  return json.dumps({'project':identity(),'dirty_maps':[o.get_path_name() for o in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]})
 @toolset_registry.tool_call
 @staticmethod
 def build_animation() -> str:
  """Import this job's two verified mesh parts and create two reusable windmill Blueprints in its unique staging folder."""
  identity();assert not unreal.EditorAssetLibrary.does_asset_exist(ROOT+'/BP_HansaWindmill_Animated')
  records=json.loads((P/'unreal_materials.json').read_text());geo=json.loads((P/'geometry.json').read_text());meshes={}
  for part in ['Body','Rotor']:
   name='SM_Windmill_'+part;assert not unreal.EditorAssetLibrary.does_asset_exist(ROOT+'/Meshes/'+name)
   opts=unreal.FbxImportUI();opts.automated_import_should_detect_type=False;opts.import_mesh=True;opts.import_as_skeletal=False;opts.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opts.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH;opts.import_materials=False;opts.import_textures=False;opts.import_animations=False;opts.static_mesh_import_data.combine_meshes=True;opts.static_mesh_import_data.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE;opts.static_mesh_import_data.auto_generate_collision=False
   mesh=import_asset(ROOT+'/Meshes',name,str(P/'exports'/f'{name}.fbx'),options=opts,factory=unreal.FbxFactory())[0]
   for slot in geo['parts'][part]['materials']:
    rec=next(r for r in records if r['name']==slot);idx=mesh.get_material_index(slot);assert idx>=0;mesh.set_material(idx,unreal.load_asset(rec['material']['refPath']))
   unreal.EditorAssetLibrary.save_loaded_asset(mesh);meshes[part]=mesh
  rotor=BlueprintTools.create(ROOT,'BP_HansaWindmill_Rotor',unreal.StaticMeshActor.static_class())
  movement=ActorTools.add_component(rotor,unreal.RotatingMovementComponent.static_class(),'SailRotation')
  movement.set_editor_property('rotation_rate',unreal.Rotator(pitch=36,yaw=0,roll=0));movement.set_editor_property('rotation_in_local_space',True);movement.set_editor_property('pivot_translation',unreal.Vector(0,0,0));movement.set_editor_property('auto_activate',True)
  rcdo=BlueprintTools.get_default_object(rotor);rcdo.static_mesh_component.set_static_mesh(meshes['Rotor']);rcdo.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE);rcdo.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
  BlueprintTools.compile_blueprint(rotor);unreal.EditorAssetLibrary.save_loaded_asset(rotor)
  building=BlueprintTools.create(ROOT,'BP_HansaWindmill_Animated',unreal.StaticMeshActor.static_class())
  child=ActorTools.add_component(building,unreal.ChildActorComponent.static_class(),'TurningSails');child.set_child_actor_class(rotor.generated_class());child.set_editor_property('relative_location',unreal.Vector(*geo['pivot_unreal_cm']));child.set_mobility(unreal.ComponentMobility.MOVABLE)
  bcdo=BlueprintTools.get_default_object(building);bcdo.static_mesh_component.set_static_mesh(meshes['Body']);bcdo.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC);bcdo.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
  BlueprintTools.compile_blueprint(building);unreal.EditorAssetLibrary.save_loaded_asset(building)
  result={'meshes':{k:{'refPath':v.get_path_name()} for k,v in meshes.items()},'blueprint':building.get_path_name(),'rotor_blueprint':rotor.get_path_name(),'rpm':6,'rotation_rate_deg_s':36,'pivot_cm':geo['pivot_unreal_cm'],'preview':LEVEL};(P/'unreal_animation.json').write_text(json.dumps(result,indent=2));return json.dumps(result)
 @toolset_registry.tool_call
 @staticmethod
 def duplicate_preview() -> str:
  """Copy the saved isolated cap-fit review level to a new animation review level without changing gameplay maps."""
  identity();assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages();assert not unreal.EditorAssetLibrary.does_asset_exist(LEVEL)
  level=unreal.EditorAssetLibrary.duplicate_asset('/Game/Hansa/Developer/GenerationPreview/HansaTowerMill_20260906_02/L_TowerPreview',LEVEL);assert level;unreal.EditorAssetLibrary.save_loaded_asset(level);return LEVEL
 @toolset_registry.tool_call
 @staticmethod
 def place_preview() -> str:
  """Replace only the old windmill actor inside this job's new isolated preview with the animated Blueprint."""
  identity();world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_path_name().startswith(LEVEL+'.')
  subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);old=[a for a in subsystem.get_all_level_actors() if a.get_name()=='StaticMeshActor_1'];assert len(old)==1
  t=old[0].get_actor_transform();bp=unreal.load_asset(ROOT+'/BP_HansaWindmill_Animated');actor=subsystem.spawn_actor_from_class(bp.generated_class(),t.translation,t.rotation.rotator());actor.set_actor_label('Hansa Windmill — Animated Sails');actor.set_editor_property('tags',['HansaAnimatedWindmill']);subsystem.destroy_actor(old[0]);unreal.EditorLoadingAndSavingUtils.save_current_level();return actor.get_path_name()
 @toolset_registry.tool_call
 @staticmethod
 def start_simulation() -> str:
  """Start Simulate in Editor in the isolated windmill animation preview for actual runtime checks."""
  identity();assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().startswith(LEVEL+'.');unreal.EditorLevelLibrary.editor_play_simulate();return 'Simulation requested'
 @toolset_registry.tool_call
 @staticmethod
 def runtime_sample() -> str:
  """Read the live rotor, fixed building, pivot, component activation and simulation clock."""
  world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert world
  building=mill(world);child=building.get_component_by_class(unreal.ChildActorComponent);rotor=child.get_child_actor();movement=rotor.get_component_by_class(unreal.RotatingMovementComponent)
  def transform(a):
   t=a.get_actor_transform();return {'location':list(t.translation.to_tuple()),'quaternion':[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w]}
  return json.dumps({'time':unreal.GameplayStatics.get_time_seconds(world),'building':transform(building),'rotor':transform(rotor),'rate':str(movement.rotation_rate),'active':movement.is_active(),'updated_component':movement.updated_component.get_name() if movement.updated_component else None})
 @toolset_registry.tool_call
 @staticmethod
 def set_test_speed(degrees_per_second: float) -> str:
  """Set the live preview rotor rate for bounded pause and reverse tests; does not edit saved defaults."""
  assert abs(degrees_per_second)<=90;world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();rotor=mill(world).get_component_by_class(unreal.ChildActorComponent).get_child_actor();rotor.get_component_by_class(unreal.RotatingMovementComponent).set_editor_property('rotation_rate',unreal.Rotator(pitch=degrees_per_second,yaw=0,roll=0));return str(degrees_per_second)
 @toolset_registry.tool_call
 @staticmethod
 def stop_simulation() -> str:
  """End only this windmill preview simulation after runtime verification."""
  identity();unreal.EditorLevelLibrary.editor_end_play();return 'Simulation ended'
_animation_registration=Registration([HansaWindmillAnimationTools]);_animation_registration.register();unreal.log('HANSA_WINDMILL_ANIMATION_TOOL_READY')
