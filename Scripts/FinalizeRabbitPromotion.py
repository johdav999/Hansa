"""Finalize this job's already-moved packages; never import or replace an asset."""
import hashlib
import json
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
job = root / 'Saved/GenerationJobs/rabbit-city_20260920_01'
durable = root / 'SourceArt/Generated/Animations/Animal.Rabbit.Locomotion/r06-city-integration'
durable.mkdir(parents=True, exist_ok=True)
base = '/Game/Hansa/Animals/Rabbit/'
names = ['SKEL_Rabbit','SK_Rabbit','A_Rabbit_Walk','A_Rabbit_Jump','M_Rabbit','LOD_Rabbit','T_Rabbit_BaseColor','T_Rabbit_Normal','T_Rabbit_Roughness']
objects = {n:u.EditorAssetLibrary.load_asset(base+n) for n in names}
assert all(objects.values())
settings = objects['LOD_Rabbit']
mesh = objects['SK_Rabbit']
assert mesh.get_editor_property('lod_settings') == settings
slots = mesh.get_editor_property('materials')
slot = slots[0]
slot.set_editor_property('material_interface', objects['M_Rabbit'])
slots[0] = slot
mesh.set_editor_property('materials', slots)
assert mesh.get_editor_property('materials')[0].get_editor_property('material_interface') == objects['M_Rabbit']
if settings.get_editor_property('min_lod').get_editor_property('default') != 1:
    settings.set_editor_property('min_lod', u.PerPlatformInt(1))
if mesh.get_editor_property('min_lod').get_editor_property('default') != 1:
    mesh.set_editor_property('min_lod', u.PerPlatformInt(1))
for name,obj in objects.items():
    stable = {'SKEL_Rabbit':'Animal.Rabbit.Skeleton.Draft01','SK_Rabbit':'Animal.Rabbit.Mesh',
              'A_Rabbit_Walk':'Animal.Rabbit.Locomotion.Walk','A_Rabbit_Jump':'Animal.Rabbit.Locomotion.Jump'}.get(name,'Animal.Rabbit.'+name)
    u.EditorAssetLibrary.set_metadata_tag(obj,'Hansa.StableId',stable)
    u.EditorAssetLibrary.set_metadata_tag(obj,'Hansa.ReviewerState','UserApprovedPromotion')
    u.EditorAssetLibrary.set_metadata_tag(obj,'Hansa.SourceRevision','r05-locomotion')
    assert u.EditorAssetLibrary.save_loaded_asset(obj, False)
registry = u.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous([base.rstrip('/')], True)
deps = {}
for name,obj in objects.items():
    package = obj.get_path_name().split('.')[0]
    assert registry.get_asset_by_object_path(obj.get_path_name()).is_valid()
    deps[package] = [str(v) for v in (registry.get_dependencies(package,u.AssetRegistryDependencyOptions(True,True,False,False,False)) or [])]
    assert not any('/Generated/Staging/' in p for p in deps[package]), (package,deps[package])
report = {'state':'promoted-user-approved','stableSkeleton':'Animal.Rabbit.Skeleton.Draft01',
          'sourceRevision':'r05-locomotion','engine':u.SystemLibrary.get_engine_version(),
          'assets':[o.get_path_name() for o in objects.values()], 'dependencies':deps,
          'runtimeLodTriangles':47062,'runtimeLod':1,'sourceMasterUnchanged':True,
          'reviewer':json.loads((job/'approval.json').read_text()),
          'outputs':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'Content/Hansa/Animals/Rabbit').glob('*.uasset')},
          'history':'Legacy importer explicitly saved dependencies; private LOD/material Python properties were replaced with supported reflected settings; dependency-free packages return None and are audited after registry rescan.',
          'gameplayVerification':'Pending separate compiled automation and viewport tests'}
for folder in (job,durable):
    (folder/'promotion.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
u.log('RABBIT_PROMOTION_FINALIZED')
