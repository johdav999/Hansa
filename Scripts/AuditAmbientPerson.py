"""Read-only audit of runtime character assets, including post-normalization hashes."""
import unreal as u, json, hashlib
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve(); base='/Game/Hansa/Characters/Laborer01'
registry=u.AssetRegistryHelpers.get_asset_registry(); registry.search_all_assets(True)
options=u.AssetRegistryDependencyOptions(); options.include_hard_package_references=True; options.include_soft_package_references=True
report={}
for path in u.EditorAssetLibrary.list_assets(base,True,False):
    a=u.load_asset(path)
    if a.get_class().get_name()=='ObjectRedirector': continue
    package=path.split('.')[0]
    deps=[str(x) for x in (registry.get_dependencies(package,options) or [])]
    assert not any('/Staging/' in x or '/Developer/' in x for x in deps),deps
    file=root/'Content'/Path(package.removeprefix('/Game/')).with_suffix('.uasset')
    info={'class':a.get_class().get_name(),'dependencies':deps,'sha256':hashlib.sha256(file.read_bytes()).hexdigest()}
    if isinstance(a,u.AnimSequence):
        info.update(seconds=a.get_editor_property('sequence_length'),frames=u.AnimationLibrary.get_num_frames(a),skeleton=a.get_editor_property('skeleton').get_path_name())
    if isinstance(a,u.SkeletalMesh):
        sub=u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
        info['lod_vertices']=[sub.get_num_verts(a,i) for i in range(sub.get_lod_count(a))]
        info['bounds']=str(a.get_bounds())
        info['materials']=[s.material_interface.get_path_name() for s in a.get_editor_property('materials')]
        assert all(x==base+'/M_Laborer01.M_Laborer01' for x in info['materials'])
    if isinstance(a,u.Material):
        info['used_with_skeletal_mesh']=a.get_editor_property('used_with_skeletal_mesh')
        assert info['used_with_skeletal_mesh']
    if isinstance(a,u.Texture2D):
        info.update(srgb=a.get_editor_property('srgb'),compression=str(a.get_editor_property('compression_settings')))
        assert info['srgb']==package.endswith('BaseColor')
    report[path]=info
(root/'Docs/Development/AmbientPeople/asset-manifest.json').write_text(json.dumps(report,indent=2))
u.log('AMBIENT_ASSET_AUDIT_PASSED')
