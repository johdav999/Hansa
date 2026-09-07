import bpy,pathlib,json,math
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1];geo=json.loads((P/'geometry.json').read_text());result={}
for part in ['Body','Rotor']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(P/'exports'/f'SM_Windmill_{part}.fbx'));obs=[o for o in bpy.context.scene.objects if o.type=='MESH'];assert len(obs)==1;o=obs[0];pts=[o.matrix_world@v.co for v in o.data.vertices];bounds=[[min(p[i] for p in pts) for i in range(3)],[max(p[i] for p in pts) for i in range(3)]];assert max(abs(bounds[a][b]-geo['parts'][part]['bounds_m'][a][b]) for a in range(2) for b in range(3))<.001;assert o.data.vertex_colors and o.data.uv_layers;assert o.location.length<.001;result[part]={'bounds_m':bounds,'pivot_at_origin':True,'vertex_colors':True,'uvs':True}
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.gltf(filepath=str(P/'exports/HansaWindmill_Animated.glb'));S=bpy.context.scene;rotor=bpy.data.objects['SM_Windmill_Rotor'];body=bpy.data.objects['SM_Windmill_Body'];print('ANIM',rotor.animation_data,[(a.name,a.frame_range[:]) for a in bpy.data.actions],flush=True);assert rotor.animation_data
rotor.animation_data.action=next(a for a in bpy.data.actions if a.name.startswith('SM_Windmill_RotorAction'));rotor.animation_data.use_nla=False
S.frame_set(1);start=rotor.matrix_world.copy();body_start=body.matrix_world.copy();S.frame_set(61);quarter=rotor.matrix_world.copy();assert (quarter.translation-start.translation).length<.001;assert abs(start.to_quaternion().rotation_difference(quarter.to_quaternion()).angle-math.pi/2)<.01;assert all(abs(body.matrix_world[i][j]-body_start[i][j])<.00001 for i in range(4) for j in range(4));S.frame_set(241);assert abs(abs(start.to_quaternion().dot(rotor.matrix_world.to_quaternion()))-1)<.001
result['glb']={'quarter_turn_degrees':90,'loop_seconds':10,'stationary_body':True,'pivot_fixed':True,'loop_seam_verified':True}
with bpy.data.libraries.load(str(P/'exports/HansaWindmill_Animated.blend'),link=False) as (a,b):b.collections=['Review']
for c in b.collections:S.collection.children.link(c)
S.world=bpy.data.worlds.new('Daylight');S.world.use_nodes=True;S.world.node_tree.nodes['Background'].inputs[0].default_value=(.72,.78,.85,1);S.world.node_tree.nodes['Background'].inputs[1].default_value=.8
S.render.engine='BLENDER_EEVEE';S.render.resolution_x=800;S.render.resolution_y=800;S.eevee.use_gtao=True;S.eevee.taa_render_samples=32;S.view_settings.view_transform='Filmic';S.view_settings.look='Medium High Contrast';S.camera=bpy.data.objects['Front'];S.camera.data.type='ORTHO';S.camera.data.ortho_scale=23;S.camera.location=(12,-34,14);S.camera.rotation_euler=(Vector((0,0,9))-S.camera.location).to_track_quat('-Z','Y').to_euler()
for f in [1,31,61]:
 S.frame_set(f);S.render.filepath=str(P/'renders'/f'glb_frame_{f}.png');bpy.ops.render.render(write_still=True)
(P/'export_verification.json').write_text(json.dumps(result,indent=2));print('EXPORT_ANIMATION_VERIFIED')
