import json
from pathlib import Path
import unreal

level = '/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP'
targets = {
 '/Game/HansaBakery_20260906_01/Meshes/SM_HansaBakery_R2.SM_HansaBakery_R2',
 '/Game/Mesh/LaborerResidence/Materials_R04/Meshes/SM_LaborerResidence.SM_LaborerResidence',
}
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert editor.load_level(level), 'Unable to load Lubeck'
descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
verified_names = {'StaticMeshActor_UAID_C87F54696D0227FF02_1405144639', 'StaticMeshActor_UAID_C87F54696D02F0FE02_2126399959'}
selected = [d.guid for d in descs if str(d.name) in verified_names]
assert len(selected) == 2, f'Expected two verified actor descriptors, found {len(selected)}'
unreal.WorldPartitionBlueprintLibrary.load_actors(selected)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
removed = []
for actor in actors.get_all_level_actors():
 if not isinstance(actor, unreal.StaticMeshActor):
  continue
 meshes = actor.get_components_by_class(unreal.StaticMeshComponent)
 if any(m.static_mesh and m.static_mesh.get_path_name() in targets for m in meshes):
  removed.append({'actor': actor.get_path_name(), 'label': actor.get_actor_label(), 'package': str(next(d.actor_package for d in descs if str(d.name) == actor.get_name()))})
  assert actors.destroy_actor(actor), 'Unable to remove showcase'
assert len(removed) == 2, f'Expected the two verified display buildings, found {len(removed)}'
assert editor.save_all_dirty_levels(), 'Unable to save Lubeck cleanup'
Path(unreal.Paths.project_saved_dir(), 'P28', 'removed-starter-showcases.json').write_text(json.dumps(removed, indent=2))
unreal.log('Removed the two static starter showcases; production mesh assets retained.')
