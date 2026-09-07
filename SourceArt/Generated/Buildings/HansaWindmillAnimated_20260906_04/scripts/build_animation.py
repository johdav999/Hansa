import bpy,pathlib,json,math,shutil,hashlib
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1];old=P.parent/'hansa-tower-capfit_20260906_03'
bpy.ops.wm.open_mainfile(filepath=str(old/'exports/HansaTowerMill_CapFit.blend'));S=bpy.context.scene
rotor=list(bpy.data.collections['Sails'].objects)+[o for o in bpy.data.collections['Hardware'].objects if o.name.startswith(('Iron_diamond','Hub_rivet','Sail_iron','Stock_binding'))]
assert len(rotor)>200
hub=bpy.data.objects['Windshaft'].location.copy();assert (hub-Vector((0,-3.39,10.82))).length<.001
# Clear the taper during the complete sweep; preserve the shaft's rear bearing endpoint.
for o in rotor:o.location.y-=.6
hub.y-=.6
shaft=bpy.data.objects['Windshaft'];shaft.location.y+=.3;shaft.scale.z*=1.85/1.25
bpy.context.view_layer.update()
asset=[o for c in S.collection.children if c.name!='Review' for o in c.objects if o.type=='MESH'];body=[o for o in asset if o not in rotor]
rig=bpy.data.objects.new('Windmill_Rotor_Pivot',None);S.collection.objects.link(rig);rig.location=hub;bpy.context.view_layer.update()
for o in rotor:
 world=o.matrix_world.copy();o.parent=rig;o.matrix_world=world
S.render.fps=24;S.frame_start=1;S.frame_end=240
for frame,angle in [(1,0),(241,2*math.pi)]:
 rig.rotation_euler.y=angle;rig.keyframe_insert(data_path='rotation_euler',frame=frame,index=1)
for curve in rig.animation_data.action.fcurves:
 for key in curve.keyframe_points:key.interpolation='LINEAR'
 curve.modifiers.new('CYCLES')
S.frame_set(1)
for im in bpy.data.images:
 if im.source=='FILE':im.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(P/'exports/HansaWindmill_Animated.blend'))
inventory=json.loads((old/'material_inventory.json').read_text())
for rec in inventory:
 m=bpy.data.materials[rec['name']];n=m.node_tree.nodes;l=m.node_tree.links;n.clear();b=n.new('ShaderNodeBsdfPrincipled');out=n.new('ShaderNodeOutputMaterial');l.new(b.outputs[0],out.inputs[0]);b.inputs['Metallic'].default_value=.55 if rec['name']=='ForgedIron' else 0
 for kind,path in rec['maps'].items():
  dest=P/'exports'/pathlib.Path(path).name;shutil.copy2(path,dest);rec['maps'][kind]=str(dest);t=n.new('ShaderNodeTexImage');t.image=bpy.data.images.load(str(dest),check_existing=True);t.image.colorspace_settings.name='sRGB' if kind=='BaseColor' else 'Non-Color'
  if kind=='Normal':
   normal=n.new('ShaderNodeNormalMap');l.new(t.outputs['Color'],normal.inputs[0]);l.new(normal.outputs[0],b.inputs['Normal'])
  else:l.new(t.outputs['Color'],b.inputs['Base Color' if kind=='BaseColor' else 'Roughness'])
(P/'material_inventory.json').write_text(json.dumps(inventory,indent=2))
info={'pivot_blender_m':list(hub),'pivot_unreal_cm':[hub.x*100,-hub.y*100,hub.z*100],'rotation_axis':'local Y / Unreal Pitch','rpm':6,'loop_seconds':10,'parts':{}}
parts=[]
for name,objects,pivot in [('Body',body,Vector((0,0,0))),('Rotor',rotor,hub)]:
 bpy.ops.object.select_all(action='DESELECT')
 for o in objects:
  w=o.matrix_world.copy();o.parent=None;o.matrix_world=w;o.select_set(True)
 bpy.context.view_layer.update();bpy.context.view_layer.objects.active=objects[0];bpy.ops.object.convert(target='MESH');bpy.ops.object.join();o=bpy.context.object;o.name='SM_Windmill_'+name
 S.cursor.location=pivot;bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=False,rotation=True,scale=True);o.location=(0,0,0)
 o.data.calc_loop_triangles();points=[v.co for v in o.data.vertices]
 info['parts'][name]={'triangles':len(o.data.loop_triangles),'vertices':len(points),'bounds_m':[[min(v[i] for v in points) for i in range(3)],[max(v[i] for v in points) for i in range(3)]],'materials':[m.name for m in o.data.materials],'uv_layers':[u.name for u in o.data.uv_layers],'vertex_colors':[v.name for v in o.data.vertex_colors]}
 bpy.ops.export_scene.fbx(filepath=str(P/'exports'/f'SM_Windmill_{name}.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',apply_unit_scale=True,path_mode='COPY',embed_textures=False,use_mesh_modifiers=True,add_leaf_bones=False,bake_anim=False)
 o.location=pivot;parts.append(o)
bpy.data.objects.remove(rig,do_unlink=True)
moving=parts[1]
for frame,angle in [(1,0),(241,2*math.pi)]:moving.rotation_euler.y=angle;moving.keyframe_insert(data_path='rotation_euler',frame=frame,index=1)
for curve in moving.animation_data.action.fcurves:
 for key in curve.keyframe_points:key.interpolation='LINEAR'
 curve.modifiers.new('CYCLES')
S.frame_end=241;S.frame_set(1);bpy.ops.object.select_all(action='DESELECT')
for o in parts:o.select_set(True)
bpy.ops.export_scene.gltf(filepath=str(P/'exports/HansaWindmill_Animated.glb'),export_format='GLB',use_selection=True,export_apply=True,export_colors=True,export_animations=True,export_frame_range=True,export_force_sampling=True)
S.frame_end=240
# Quantitative swept envelope and stationary body checks over a full revolution.
minimum_z=1e9;startbody=parts[0].matrix_world.copy();pivot_error=0
for f in range(1,242):
 S.frame_set(f);bpy.context.view_layer.update();pivot_error=max(pivot_error,(moving.matrix_world.translation-hub).length)
 minimum_z=min(minimum_z,min((moving.matrix_world@v.co).z for v in moving.data.vertices))
 assert all(abs(parts[0].matrix_world[i][j]-startbody[i][j])<1e-8 for i in range(4) for j in range(4))
info['verification']={'pivot_drift_m':pivot_error,'minimum_rotor_height_m':minimum_z,'body_stationary':True,'frames_checked':241};print('SWEEP',json.dumps(info),flush=True);assert minimum_z>2 and pivot_error<1e-6
(P/'geometry.json').write_text(json.dumps(info,indent=2))
for m in bpy.data.materials:
 if m.name not in [r['name'] for r in inventory]:continue
 n=m.node_tree.nodes;l=m.node_tree.links;b=n.get('Principled BSDF');src=b.inputs['Base Color'].links[0].from_socket;vc=n.new('ShaderNodeVertexColor');vc.layer_name='Weathering';mix=n.new('ShaderNodeMixRGB');mix.blend_type='MULTIPLY';mix.inputs[0].default_value=1;l.new(src,mix.inputs[1]);l.new(vc.outputs[0],mix.inputs[2]);l.new(mix.outputs[0],b.inputs['Base Color'])
S.frame_set(1);bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints/portable_animated.blend'))
S.camera=bpy.data.objects['Front'];S.render.resolution_x=800;S.render.resolution_y=800;S.eevee.taa_render_samples=32
S.camera.data.type='ORTHO';S.camera.data.ortho_scale=23;S.camera.location=(12,-34,14);S.camera.rotation_euler=(Vector((0,0,9))-S.camera.location).to_track_quat('-Z','Y').to_euler()
(P/'renders/animation').mkdir(exist_ok=True)
for frame in range(120):
 S.frame_set(1+2*frame);S.render.filepath=str(P/'renders/animation'/f'{frame:03}.png');bpy.ops.render.render(write_still=True)
print('ANIMATION_EXPORTED',json.dumps(info),flush=True)
