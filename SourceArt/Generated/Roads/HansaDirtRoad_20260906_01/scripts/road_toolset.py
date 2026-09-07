import unreal,toolset_registry,json,math
from pathlib import Path
from toolset_registry.registration import Registration
from editor_toolset.toolsets.asset import import_asset
from editor_toolset.toolsets.actor import ActorTools
from editor_toolset.toolsets.blueprint import BlueprintTools
P=Path(__file__).resolve().parents[1]
ROOT='/Game/Hansa/Generated/Staging/HansaDirtRoad_20260906_01'
LEVEL='/Game/Hansa/Developer/GenerationPreview/HansaDirtRoad_20260906_01/L_DirtRoad_Review'
def identity():
 p=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
 assert p==(P.parents[2]/'Hansa.uproject').resolve(),str(p)
 return str(p)
def record(name,data):
 (P/'evidence'/name).write_text(json.dumps(data,indent=2));return json.dumps(data)
@unreal.uclass()
class HansaDirtRoadToolsV3(unreal.ToolsetDefinition):
 @toolset_registry.tool_call
 @staticmethod
 def identity() -> str:
  """Verify exact Hansa project, dirty maps and this road staging folder before mutation."""
  return record('unreal_identity.json',{'project':identity(),'level':unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name(),'dirty_maps':[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],'existing':list(unreal.EditorAssetLibrary.list_assets(ROOT))})
 @toolset_registry.tool_call
 @staticmethod
 def import_roads() -> str:
  """Import only this job's five verified road FBXs and shared PBR maps into a fresh staging folder; no overwrite."""
  try:
   identity()
   at=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary
   m=unreal.load_asset(ROOT+'/Materials/M_DirtRoad');assert m;ml.delete_all_material_expressions(m)
   m.set_editor_property('used_with_spline_meshes',True)
   files={'BaseColor':'dirt-road--basecolor--v1.png','Normal':'Dirt_Normal.png','Roughness':'Dirt_Roughness.png'};nodes={};textures={}
   for kind,f in files.items():
    t=unreal.AssetImportTask();t.filename=str(P/'textures'/f);t.destination_path=ROOT+'/Textures';t.destination_name='T_DirtRoad_'+kind;t.automated=True;t.replace_existing=False;t.save=False;assert unreal.EditorAssetLibrary.does_asset_exist(ROOT+'/Textures/T_DirtRoad_'+kind)
    tex=unreal.load_asset(ROOT+'/Textures/T_DirtRoad_'+kind);tex.set_editor_property('srgb',kind=='BaseColor')
    if kind=='Normal':tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP);tex.set_editor_property('flip_green_channel',True)
    elif kind=='Roughness':tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
    n=ml.create_material_expression(m,unreal.MaterialExpressionTextureSample,-600,len(nodes)*220);n.texture=tex;n.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if kind=='Normal' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if kind=='Roughness' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    nodes[kind]=n;textures[kind]=tex.get_path_name();unreal.EditorAssetLibrary.save_loaded_asset(tex)
   vc=ml.create_material_expression(m,unreal.MaterialExpressionVertexColor,-600,-200);mul=ml.create_material_expression(m,unreal.MaterialExpressionMultiply,-200,0)
   assert ml.connect_material_expressions(nodes['BaseColor'],'RGB',mul,'A');assert ml.connect_material_expressions(vc,'',mul,'B');assert ml.connect_material_property(mul,'',unreal.MaterialProperty.MP_BASE_COLOR)
   assert ml.connect_material_property(nodes['Roughness'],'R',unreal.MaterialProperty.MP_ROUGHNESS)
   # Match source normal strength through XY scale and normalization.
   scale=ml.create_material_expression(m,unreal.MaterialExpressionMultiply,-250,300);k=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector,-500,600);k.constant=unreal.LinearColor(.5,.5,1,1)
   norm=ml.create_material_expression(m,unreal.MaterialExpressionNormalize,-100,300)
   ml.connect_material_expressions(nodes['Normal'],'RGB',scale,'A');ml.connect_material_expressions(k,'',scale,'B');ml.connect_material_expressions(scale,'',norm,'VectorInput');ml.connect_material_property(norm,'',unreal.MaterialProperty.MP_NORMAL)
   spec=ml.create_material_expression(m,unreal.MaterialExpressionConstant,-200,700);spec.r=.22;ml.connect_material_property(spec,'',unreal.MaterialProperty.MP_SPECULAR)
   ml.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
   opts=unreal.FbxImportUI();opts.automated_import_should_detect_type=False;opts.import_mesh=True;opts.import_as_skeletal=False;opts.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opts.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH;opts.import_materials=False;opts.import_textures=False;opts.import_animations=False
   data=opts.static_mesh_import_data;data.combine_meshes=True;data.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE;data.auto_generate_collision=False;data.generate_lightmap_u_vs=True;data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
   meshes={}
   for f in sorted((P/'exports').glob('SM_*.fbx')):
    assert not unreal.EditorAssetLibrary.does_asset_exist(ROOT+'/Meshes/'+f.stem)
    mesh=import_asset(ROOT+'/Meshes',f.stem,str(f),options=opts,factory=unreal.FbxFactory())[0];assert mesh
    mesh.set_material(0,m)
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    unreal.EditorAssetLibrary.set_metadata_tag(mesh,'Hansa.Generation.Status','DraftReview')
    unreal.EditorAssetLibrary.set_metadata_tag(mesh,'Hansa.Generation.Job','hansa-dirt-road_20260906_01')
    unreal.EditorAssetLibrary.save_loaded_asset(mesh);meshes[f.stem]=mesh.get_path_name()
   assert len(meshes)==5
   return record('unreal_import.json',{'meshes':meshes,'material':m.get_path_name(),'textures':textures,'level':LEVEL})
  except Exception:
   import traceback
   return record('import_error.json',{'error':traceback.format_exc()})
 @toolset_registry.tool_call
 @staticmethod
 def create_preview() -> str:
  """Create this road kit's isolated review map and a genuine three-segment curved rising spline example, preserving other saved maps."""
  identity();assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages();assert not unreal.EditorAssetLibrary.does_asset_exist(LEVEL)
  world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False);sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
  geo=json.loads((P/'evidence/unreal_import.json').read_text());positions={'Straight_8m':(-900,-1000,0),'Corner90_R6m':(700,-500,0),'End_5m':(-900,0,0),'TJunction_12m':(-900,1200,0),'Crossroads_12m':(800,1200,0)}
  for name,path in geo['meshes'].items():
   a=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*positions[name.removeprefix('SM_DirtRoad_')]))
   a.static_mesh_component.set_static_mesh(unreal.load_asset(path));a.set_actor_label(name);a.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC)
  # Reusable saved example: native SplineComponent and three native SplineMeshComponents.
  bp=BlueprintTools.create(ROOT,'BP_DirtRoad_SplineExample',unreal.Actor.static_class())
  spline=ActorTools.add_component(bp,unreal.SplineComponent.static_class(),'RoadPath')
  pts=[unreal.Vector(0,0,0),unreal.Vector(700,180,50),unreal.Vector(1400,-100,100),unreal.Vector(2100,0,0)]
  spline.set_spline_points(pts,unreal.SplineCoordinateSpace.LOCAL,True)
  samples=[]
  for i in range(3):
   sm=ActorTools.add_component(bp,unreal.SplineMeshComponent.static_class(),'RoadSegment'+str(i));sm.set_static_mesh(unreal.load_asset(geo['meshes']['SM_DirtRoad_Straight_8m']));sm.set_forward_axis(unreal.SplineMeshAxis.X,False)
   p0=spline.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.LOCAL);p1=spline.get_location_at_spline_point(i+1,unreal.SplineCoordinateSpace.LOCAL);t0=spline.get_tangent_at_spline_point(i,unreal.SplineCoordinateSpace.LOCAL);t1=spline.get_tangent_at_spline_point(i+1,unreal.SplineCoordinateSpace.LOCAL)
   sm.set_start_and_end(p0,t0,p1,t1,True);sm.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
   samples.append({'start':list(p0.to_tuple()),'end':list(p1.to_tuple()),'start_tangent':list(t0.to_tuple()),'end_tangent':list(t1.to_tuple())})
  BlueprintTools.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp)
  demo=sub.spawn_actor_from_class(bp.generated_class(),unreal.Vector(-300,-1500,0));demo.set_actor_label('DirtRoad native spline example')
  # Review-only ground is deliberately lower than the edges; landscape blending is evaluated separately.
  floor=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-27));floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));floor.set_actor_scale3d(unreal.Vector(100,100,.1));floor.set_actor_label('Review Ground')
  gm=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_ReviewGround',ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew());ml=unreal.MaterialEditingLibrary
  c=ml.create_material_expression(gm,unreal.MaterialExpressionConstant3Vector,0,0);c.constant=unreal.LinearColor(.10,.12,.10,1);ml.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR);ml.recompile_material(gm);unreal.EditorAssetLibrary.save_loaded_asset(gm);floor.static_mesh_component.set_material(0,gm)
  sun=sub.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,1500),unreal.Rotator(pitch=-48,yaw=-32,roll=0));sun.light_component.set_editor_property('intensity',4.0)
  sky=sub.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,1200));sky.light_component.set_editor_property('intensity',.8);sky.light_component.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP);sky.light_component.set_editor_property('cubemap',unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
  pp=sub.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector());pp.set_editor_property('unbound',True);settings=pp.get_editor_property('settings');settings.set_editor_property('override_auto_exposure_method',True);settings.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL);settings.set_editor_property('override_auto_exposure_bias',True);settings.set_editor_property('auto_exposure_bias',10.0);pp.set_editor_property('settings',settings)
  assert unreal.EditorLoadingAndSavingUtils.save_map(world,LEVEL)
  return record('unreal_preview.json',{'level':LEVEL,'blueprint':bp.get_path_name(),'samples':samples,'sun_rotation':str(sun.get_actor_rotation())})
 @toolset_registry.tool_call
 @staticmethod
 def verify() -> str:
  """Read saved road bounds, material assignments, map state and genuine spline components after reload."""
  identity();geo=json.loads((P/'evidence/unreal_import.json').read_text());rows=[]
  for name,path in geo['meshes'].items():
   mesh=unreal.load_asset(path);b=mesh.get_bounding_box();rows.append({'name':name,'bounds_cm':{'min':list(b.min.to_tuple()),'max':list(b.max.to_tuple())},'material':mesh.get_material(0).get_path_name(),'vertices':unreal.EditorStaticMeshLibrary.get_number_verts(mesh,0),'uv_channels':unreal.EditorStaticMeshLibrary.get_num_uv_channels(mesh,0)})
  actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();splines=[]
  for a in actors:
   for c in a.get_components_by_class(unreal.SplineMeshComponent):splines.append({'path':c.get_path_name(),'mesh':c.static_mesh.get_path_name(),'start':list(c.get_start_position().to_tuple()),'end':list(c.get_end_position().to_tuple()),'forward':str(c.get_forward_axis()),'registered':c.is_registered() if hasattr(c,'is_registered') else 'unknown'})
  assert len(splines)==3
  return record('unreal_verification.json',{'meshes':rows,'spline_components':splines,'dirty_maps':[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]})
_road_registration=Registration([HansaDirtRoadToolsV3]);_road_registration.register();unreal.log('HANSA_DIRT_ROAD_TOOL_READY')


