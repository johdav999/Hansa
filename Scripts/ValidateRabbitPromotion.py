"""Read-only clean-process audit of promoted rabbit packages and raw root tracks."""
import hashlib
import json
import math
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
out = root / 'Saved/GenerationJobs/rabbit-city_20260920_01'
base = '/Game/Hansa/Animals/Rabbit/'
lib = u.EditorAssetLibrary
mesh = lib.load_asset(base + 'SK_Rabbit')
skeleton = lib.load_asset(base + 'SKEL_Rabbit')
assert mesh and skeleton and mesh.get_editor_property('skeleton') == skeleton
assert mesh.get_editor_property('materials')[0].get_editor_property('material_interface') == lib.load_asset(base+'M_Rabbit')
subsystem = u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
assert subsystem.get_lod_count(mesh) == 2
vertices = subsystem.get_num_verts(mesh, 1)
assert 1000 < vertices < 100000
assert mesh.get_editor_property('min_lod').get_editor_property('default') == 1
report = {'engine': u.SystemLibrary.get_engine_version(), 'cleanLoad': True,
          'lodCount': 2, 'runtimeLodVertices': vertices, 'clips': {}, 'dependencies': {}}
for name in ('Walk','Jump'):
    clip = lib.load_asset(base + 'A_Rabbit_' + name)
    assert clip and clip.get_editor_property('skeleton') == skeleton
    keys = u.AnimationLibrary.get_num_keys(clip)
    length = u.AnimationLibrary.get_sequence_length(clip)
    assert keys == 31 and abs(length-1) < .001
    tracks = [str(n) for n in u.AnimationLibrary.get_animation_track_names(clip)]
    assert len(tracks) == 25 and tracks[0] == 'root'
    roots = []
    for frame in range(31):
        transform = u.AnimationLibrary.extract_root_track_transform(clip, frame/30)
        pos = transform.translation
        scale = transform.scale3d
        assert max(abs(scale.x-1),abs(scale.y-1),abs(scale.z-1)) < .001
        roots.append([pos.x,pos.y,pos.z])
    delta = [roots[-1][i]-roots[0][i] for i in range(3)]
    if name == 'Walk':
        assert max(abs(v-roots[0][i]) for p in roots for i,v in enumerate(p)) < .01
    else:
        assert abs(math.hypot(delta[0],delta[1])-45) < .5 and abs(delta[2]) < .5
        assert abs(max(p[2]-roots[0][2] for p in roots)-23) < 1
        assert clip.get_editor_property('enable_root_motion') and clip.get_editor_property('force_root_lock')
    report['clips'][name] = {'keys':keys,'durationSeconds':length,'tracks':tracks,'rootDisplacementCm':delta,'rootSamplesCm':roots}
registry = u.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous([base.rstrip('/')], True)
for path in lib.list_assets(base, recursive=True, include_folder=False):
    package = path.split('.')[0]
    deps = [str(n) for n in (registry.get_dependencies(package, u.AssetRegistryDependencyOptions(True,True,False,False,False)) or [])]
    assert not any('/Generated/Staging/' in p for p in deps), (path,deps)
    report['dependencies'][package] = deps
report['outputHashes'] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'Content/Hansa/Animals/Rabbit').glob('*.uasset')}
(out/'clean-load-validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
(root/'SourceArt/Generated/Animations/Animal.Rabbit.Locomotion/r06-city-integration/clean-load-validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
u.log('RABBIT_CLEAN_LOAD_VALIDATION_PASSED')
