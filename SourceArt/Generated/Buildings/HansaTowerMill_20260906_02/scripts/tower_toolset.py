import unreal,toolset_registry,json
from pathlib import Path
from toolset_registry.registration import Registration
from editor_toolset.toolsets.asset import import_asset
P=Path(__file__).resolve().parents[1];ROOT='/Game/Hansa/Generated/Staging/HansaTowerMill_20260906_02'
@unreal.uclass()
class HansaTowerMillTools(unreal.ToolsetDefinition):
 @toolset_registry.tool_call
 @staticmethod
 def identity() -> str:
  """Read this editor project and dirty map state before the bounded tower mill import."""
  return json.dumps({'project':unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path()),'dirty_maps':[o.get_path_name() for o in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]})
 @toolset_registry.tool_call
 @staticmethod
 def import_tower() -> str:
  """Import only this job's verified tower FBX, maps and seven materials into its new staging folder. Never overwrite assets."""
  assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()==(P.parents[2]/'Hansa.uproject').resolve()
  assert not unreal.EditorAssetLibrary.does_asset_exist(ROOT+'/Meshes/SM_HansaTowerMill')
  opts=unreal.FbxImportUI();opts.automated_import_should_detect_type=False;opts.import_mesh=True;opts.import_as_skeletal=False;opts.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opts.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH;opts.import_materials=False;opts.import_textures=False;opts.import_animations=False;opts.static_mesh_import_data.combine_meshes=True;opts.static_mesh_import_data.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
  mesh=import_asset(ROOT+'/Meshes','SM_HansaTowerMill',str(P/'exports/HansaTowerMill.fbx'),options=opts,factory=unreal.FbxFactory())[0]
  records=[];ml=unreal.MaterialEditingLibrary;at=unreal.AssetToolsHelpers.get_asset_tools()
  for rec in json.loads((P/'material_inventory.json').read_text()):
   name=rec['name'];m=at.create_asset('M_Tower_'+name,ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew());assert m
   textures={};nodes={}
   for kind,path in rec['maps'].items():
    task=unreal.AssetImportTask();task.filename=path;task.destination_path=ROOT+'/Textures';task.destination_name='T_Tower_'+name+'_'+kind;task.automated=True;task.replace_existing=False;task.save=False;at.import_asset_tasks([task]);tex=unreal.load_asset(task.imported_object_paths[0]);tex.set_editor_property('srgb',kind=='BaseColor')
    if kind=='Normal':tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP);tex.set_editor_property('flip_green_channel',True)
    elif kind=='Roughness':tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
    tex.post_edit_change() if hasattr(tex,'post_edit_change') else None
    node=ml.create_material_expression(m,unreal.MaterialExpressionTextureSample,-600,len(nodes)*220);node.texture=tex
    node.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if kind=='Normal' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if kind=='Roughness' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    nodes[kind]=node;textures[kind]={'refPath':tex.get_path_name()};unreal.EditorAssetLibrary.save_loaded_asset(tex)
   vc=ml.create_material_expression(m,unreal.MaterialExpressionVertexColor,-600,-200);mul=ml.create_material_expression(m,unreal.MaterialExpressionMultiply,-250,0)
   assert ml.connect_material_expressions(nodes['BaseColor'],'RGB',mul,'A');assert ml.connect_material_expressions(vc,'',mul,'B');assert ml.connect_material_property(mul,'',unreal.MaterialProperty.MP_BASE_COLOR)
   assert ml.connect_material_property(nodes['Roughness'],'R',unreal.MaterialProperty.MP_ROUGHNESS);assert ml.connect_material_property(nodes['Normal'],'RGB',unreal.MaterialProperty.MP_NORMAL)
   metal=ml.create_material_expression(m,unreal.MaterialExpressionConstant,-200,600);metal.r=.55 if name=='ForgedIron' else 0;ml.connect_material_property(metal,'',unreal.MaterialProperty.MP_METALLIC);ml.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
   index=mesh.get_material_index(name);assert index>=0;mesh.set_material(index,m);records.append({'name':name,'material':{'refPath':m.get_path_name()},'textures':textures})
  unreal.EditorAssetLibrary.save_loaded_asset(mesh);(P/'unreal_materials.json').write_text(json.dumps(records,indent=2));(P/'unreal_mesh.json').write_text(json.dumps({'refPath':mesh.get_path_name()}))
  return mesh.get_path_name()
 @toolset_registry.tool_call
 @staticmethod
 def create_preview() -> str:
  """Duplicate the previously saved mill development preview into this job's unique preview and replace only its mill component."""
  assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
  old='/Game/Hansa/Developer/GenerationPreview/HansaMill_20260906_01/L_MillPreview';new='/Game/Hansa/Developer/GenerationPreview/HansaTowerMill_20260906_02/L_TowerPreview'
  assert not unreal.EditorAssetLibrary.does_asset_exist(new);level=unreal.EditorAssetLibrary.duplicate_asset(old,new);assert level;unreal.EditorAssetLibrary.save_loaded_asset(level);del level;assert unreal.EditorLoadingAndSavingUtils.load_map(new)
  mesh=unreal.load_asset(json.loads((P/'unreal_mesh.json').read_text())['refPath']);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();found=None
  for a in actors:
   if a.get_name()=='StaticMeshActor_1':a.static_mesh_component.set_static_mesh(mesh);a.set_actor_location(unreal.Vector(0,0,0),False,False);found=a
  assert found;unreal.EditorLoadingAndSavingUtils.save_current_level();info={'level':new,'model':{'refPath':found.get_path_name()}};(P/'preview_actors.json').write_text(json.dumps(info,indent=2));return json.dumps(info)
_tower_registration=Registration([HansaTowerMillTools]);_tower_registration.register();unreal.log('HANSA_TOWER_TOOL_READY')
