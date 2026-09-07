import bpy,math,json,sys
from pathlib import Path
from mathutils import Vector
P=Path(__file__).resolve().parents[1]
S=bpy.context.scene
ob=bpy.data.objects['SM_GrainFieldPatch_4m']; soil=bpy.data.objects['SM_GrainFieldSoil_4m']
bpy.context.view_layer.objects.active=ob; ob.select_set(True)
# Weld per-face construction duplicates without altering the silhouette or UV seams.
bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True)
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.mesh.remove_doubles(threshold=.00001); bpy.ops.mesh.normals_make_consistent(inside=False); bpy.ops.object.mode_set(mode='OBJECT')
ob.shape_key_add(name='Basis')
a=ob.shape_key_add(name='Wind_Sine'); b=ob.shape_key_add(name='Wind_Cosine')
for key in [a,b]:key.slider_min=-1;key.slider_max=1
for i,v in enumerate(ob.data.vertices):
 x,y,z=v.co; w=min(1,max(0,z/1.35))**2
 # Integer spatial periods in a 4m tile: opposite boundaries share phase.
 p=math.tau*(x+y)/4
 a.data[i].co=v.co+Vector((.11*w*math.cos(p),.035*w*math.cos(p+.6),-.008*w))
 b.data[i].co=v.co+Vector((.11*w*math.sin(p),.035*w*math.sin(p+.6),0))
for f in range(1,194,4):
 t=math.tau*(f-1)/192; a.value=math.sin(t);b.value=math.cos(t)
 a.keyframe_insert('value',frame=f);b.keyframe_insert('value',frame=f)
for fc in ob.data.shape_keys.animation_data.action.fcurves:
 fc.modifiers.new('CYCLES')
 for k in fc.keyframe_points:k.interpolation='LINEAR'
S.render.fps=24; S.frame_start=1; S.frame_end=193; S.frame_set(1)
for m in ob.data.materials:
 m.use_backface_culling=False
# Pack source procedural materials and native images.
bpy.ops.file.pack_all(); bpy.ops.wm.save_as_mainfile(filepath=str(P/'exports'/'GrainFieldPatch_Animated_Source.blend'))
(P/'wind_contract.json').write_text(json.dumps({'duration_seconds':8,'fps':24,'frames':192,'amplitude_m':[.11,.035],'root_motion':False,'phase':'2*pi*(local_x+local_y)/4; periodic across 4m edges','root_weight':'clamp(z/1.35,0,1)^2','portable_animation':'GLB morph targets','unreal_animation':'world-position-offset material using the same analytical wave; requires verified UE implementation','FBX':'static geometry; wind material required'},indent=2))
# Bake full material swatches at the native color input dimensions.
S.cycles.samples=1
for m in list(ob.data.materials)+list(soil.data.materials):
 bpy.ops.object.select_all(action='DESELECT')
 bpy.ops.mesh.primitive_plane_add(size=1 if 'Soil' in m.name else .1,location=(0,0,-20))
 sw=bpy.context.object;sw.data.materials.append(m)
 n=m.node_tree.nodes;l=m.node_tree.links;bs=n.get('Principled BSDF')
 baked={}
 for channel in ['BaseColor','Roughness','Normal']:
  im=bpy.data.images.new(m.name+'_'+channel,1254,1254,alpha=False); im.filepath_raw=str(P/'textures'/(im.name+'.png'));im.file_format='PNG'
  if channel!='BaseColor':im.colorspace_settings.name='Non-Color'
  dest=n.new('ShaderNodeTexImage');dest.image=im;n.active=dest
  if channel=='BaseColor':bpy.ops.object.bake(type='DIFFUSE',pass_filter={'COLOR'},margin=0)
  elif channel=='Normal':bpy.ops.object.bake(type='NORMAL',margin=0)
  else:bpy.ops.object.bake(type='ROUGHNESS',margin=0)
  im.save();baked[channel]=dest
 l.new(baked['BaseColor'].outputs['Color'],bs.inputs['Base Color']);l.new(baked['Roughness'].outputs['Color'],bs.inputs['Roughness'])
 nm=n.new('ShaderNodeNormalMap');l.new(baked['Normal'].outputs['Color'],nm.inputs['Color']);l.new(nm.outputs['Normal'],bs.inputs['Normal'])
 bpy.data.objects.remove(sw,do_unlink=True)
S.cycles.samples=24
bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);soil.select_set(True)
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(P/'exports'/'GrainFieldPatch_Animated_Portable.blend'))
bpy.ops.export_scene.gltf(filepath=str(P/'exports'/'GrainFieldPatch_Animated.glb'),export_format='GLB',use_selection=True,export_animations=True,export_frame_range=True,export_morph=True,export_colors=False)
bpy.ops.export_scene.fbx(filepath=str(P/'exports'/'GrainFieldPatch_StaticForWindMaterial.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,path_mode='COPY',embed_textures=True)
# Source scene evidence with analytical wind, hero frame and tiled arrangement.
S.camera=bpy.data.objects['Review_Hero'];S.render.filepath=str(P/'renders'/'animated_hero.jpg');bpy.ops.render.render(write_still=True)
for x,y in [(4,0),(0,4),(4,4)]:
 for source in [ob,soil]:
  du=source.copy();du.data=source.data;S.collection.objects.link(du);du.location.x+=x;du.location.y+=y
c=S.camera;c.location=(10,-10,10);c.rotation_euler=(Vector((2,2,.55))-c.location).to_track_quat('-Z','Y').to_euler();c.data.ortho_scale=12
S.render.filepath=str(P/'renders'/'tiled_2x2.jpg');bpy.ops.render.render(write_still=True)
(P/'export_stats.json').write_text(json.dumps({'vertices_welded':len(ob.data.vertices),'triangles':sum(len(p.vertices)-2 for p in ob.data.polygons),'shape_keys':len(ob.data.shape_keys.key_blocks),'packed_images':all(im.packed_file is not None for im in bpy.data.images if im.source=='FILE'),'source_bounds_m':[list(ob.dimensions),list(soil.dimensions)]},indent=2))
