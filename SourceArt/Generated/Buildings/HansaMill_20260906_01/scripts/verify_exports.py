import bpy,pathlib,json,sys,math
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1];fmt=sys.argv[sys.argv.index('--')+1]
bpy.ops.wm.read_factory_settings(use_empty=True)
if fmt=='glb':bpy.ops.import_scene.gltf(filepath=str(P/'exports/HansaMill.glb'))
else:bpy.ops.import_scene.fbx(filepath=str(P/'exports/HansaMill.fbx'))
asset=[o for o in bpy.context.scene.objects if o.type=='MESH']
assert asset
pts=[o.matrix_world@v.co for o in asset for v in o.data.vertices];bounds=[[min(p[k] for p in pts) for k in range(3)],[max(p[k] for p in pts) for k in range(3)]]
expected=json.loads((P/'exports/geometry.json').read_text())['bounds_m'];assert max(abs(bounds[i][j]-expected[i][j]) for i in range(2) for j in range(3))<.005
info={'format':fmt,'bounds_m':bounds,'mesh_count':len(asset),'vertex_color_layers':[[v.name for v in o.data.vertex_colors] for o in asset],'images':[{'name':im.name,'dimensions':list(im.size)} for im in bpy.data.images if im.type=='IMAGE'],'materials':list(set(m.name for o in asset for m in o.data.materials))}
with bpy.data.libraries.load(str(P/'exports/HansaMill.blend'),link=False) as (a,b):b.collections=['Review']
for c in b.collections:bpy.context.scene.collection.children.link(c)
S=bpy.context.scene;S.world=bpy.data.worlds.new('Neutral daylight');S.world.use_nodes=True;S.world.node_tree.nodes['Background'].inputs[0].default_value=(.67,.76,.88,1);S.world.node_tree.nodes['Background'].inputs[1].default_value=.7
S.render.engine='BLENDER_EEVEE';S.eevee.use_gtao=True;S.eevee.gtao_distance=2;S.eevee.gtao_factor=1.08;S.eevee.taa_render_samples=64;S.eevee.use_soft_shadows=True;S.render.resolution_x=1200;S.render.resolution_y=1200;S.render.resolution_percentage=100;S.view_settings.view_transform='Filmic';S.view_settings.look='Medium High Contrast';S.render.image_settings.file_format='PNG'
for name in ['Hero','Base_Detail','Roof_Detail']:
 S.camera=bpy.data.objects[name];S.render.filepath=str(P/'renders'/f'reimport_{fmt}_{name}.png');bpy.ops.render.render(write_still=True)
(P/'exports'/f'reimport_{fmt}.json').write_text(json.dumps(info,indent=2));print('CLEAN_REIMPORT_VERIFIED',fmt,flush=True)
