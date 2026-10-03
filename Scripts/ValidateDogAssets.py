import unreal as u,json
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve()
r=u.AssetRegistryHelpers.get_asset_registry();r.search_all_assets(True)
options=u.AssetRegistryDependencyOptions();options.include_hard_package_references=True;options.include_soft_package_references=True
paths=u.EditorAssetLibrary.list_assets('/Game/Hansa/Animals/Dog',True,False)
report={}
for path in paths:
 asset=u.EditorAssetLibrary.load_asset(path)
 package=path.split('.')[0]
 deps=[str(x) for x in r.get_dependencies(package,options)]
 if isinstance(asset,(u.SkeletalMesh,u.AnimSequence,u.Material)): assert deps,package
 assert not any('/Generated/Staging/' in x for x in deps),deps
 report[package]={'class':asset.get_class().get_name(),'dependencies':deps}
 if isinstance(asset,u.SkeletalMesh):
  subsystem=u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
  report[package]['section_materials']=[]
  for i in range(subsystem.get_num_sections(asset,0)):
   slot=subsystem.get_lod_material_slot(asset,0,i);material=asset.get_editor_property('materials')[slot].material_interface
   assert material and material.get_path_name()=='/Game/Hansa/Animals/Dog/M_Dog.M_Dog'
   report[package]['section_materials'].append(material.get_path_name())
 if isinstance(asset,u.Texture2D):
  assert asset.get_editor_property('srgb') == package.endswith('BaseColor')
  if package.endswith('Normal'): assert asset.get_editor_property('flip_green_channel') and asset.get_editor_property('compression_settings') == u.TextureCompressionSettings.TC_NORMALMAP
  report[package].update(srgb=asset.get_editor_property('srgb'),compression=str(asset.get_editor_property('compression_settings')),flip_green=asset.get_editor_property('flip_green_channel'))
(root/'SourceArt/Generated/Animals/DogMaterial_20260923/unreal-integration/dependency-audit.json').write_text(json.dumps(report,indent=2))
p=root/'SourceArt/Generated/Animals/DogMaterial_20260923/unreal-integration/promotion.json'
promotion=json.loads(p.read_text());promotion['dependencies']={k:v['dependencies'] for k,v in report.items()};p.write_text(json.dumps(promotion,indent=2))
u.log('DOG_DEPENDENCY_AUDIT_PASSED')

