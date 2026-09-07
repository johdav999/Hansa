import bpy,json,sys,math
from pathlib import Path
from mathutils import Vector
P=Path(__file__).resolve().parents[1]
kind=sys.argv[sys.argv.index('--')+1]
src=P/'exports'/'GrainFieldPatch_Animated_Portable.blend'
bpy.ops.wm.open_mainfile(filepath=str(src))
S=bpy.context.scene
source=bpy.data.objects['SM_GrainFieldPatch_4m']
source_dims=list(source.dimensions)
if kind in ['glb','fbx']:
 for o in list(bpy.data.objects):
  if o.type=='MESH':bpy.data.objects.remove(o,do_unlink=True)
 for m in list(bpy.data.materials):bpy.data.materials.remove(m,do_unlink=True)
 for im in list(bpy.data.images):
  if im.source=='FILE':bpy.data.images.remove(im,do_unlink=True)
 if kind=='glb':bpy.ops.import_scene.gltf(filepath=str(P/'exports'/'GrainFieldPatch_Animated.glb'))
 else:bpy.ops.import_scene.fbx(filepath=str(P/'exports'/'GrainFieldPatch_StaticForWindMaterial.fbx'))
meshes=[o for o in S.objects if o.type=='MESH']
crop=max(meshes,key=lambda o:len(o.data.vertices));ground=min(meshes,key=lambda o:len(o.data.vertices))
assert abs(ground.dimensions.x-4)<.001 and abs(ground.dimensions.y-4)<.001
assert crop.dimensions.z>.8 and crop.dimensions.z<1.6
results={'format':kind,'objects':[o.name for o in meshes],'dimensions_m':{o.name:list(o.dimensions) for o in meshes},'material_slots':{o.name:[m.name if m else None for m in o.data.materials] for o in meshes},'maps':{im.name:list(im.size) for im in bpy.data.images if im.source=='FILE'},'uv_channels':len(crop.data.uv_layers)}
if crop.data.shape_keys and crop.data.shape_keys.animation_data:
 def evaluated(f):
  S.frame_set(f);dg=bpy.context.evaluated_depsgraph_get();ev=crop.evaluated_get(dg);m=ev.to_mesh();a=[v.co.copy() for v in m.vertices];ev.to_mesh_clear();return a
 a=evaluated(1);b=evaluated(49);c=evaluated(193)
 results['max_wind_displacement_m']=max((x-y).length for x,y in zip(a,b));results['loop_position_error_m']=max((x-y).length for x,y in zip(a,c))
 results['quarter_cycle_weights']={}
 for frame,expected in [(1,[.5,1]),(49,[1,.5]),(97,[.5,0]),(145,[0,.5]),(193,[.5,1])]:
  S.frame_set(frame);actual=[k.value for k in crop.data.shape_keys.key_blocks][1:];results['quarter_cycle_weights'][str(frame)]=actual;assert max(abs(x-y) for x,y in zip(actual,expected))<.0001
 roots=[i for i,x in enumerate(a) if abs(x.z)<.001]
 results['root_drift_m']=max(((a[i]-b[i]).length for i in roots),default=0);results['root_samples']=len(roots)
 assert results['max_wind_displacement_m']>.04
 assert results['loop_position_error_m']<.0001
 assert results['root_drift_m']<.00001
S.frame_set(1);S.camera=bpy.data.objects['Review_Hero'];S.render.filepath=str(P/'renders'/(kind+'_reimport_hero.jpg'));bpy.ops.render.render(write_still=True)
S.camera=bpy.data.objects['Review_Close'];S.render.filepath=str(P/'renders'/(kind+'_reimport_close.jpg'));bpy.ops.render.render(write_still=True)
(P/(kind+'_verification.json')).write_text(json.dumps(results,indent=2))
print(json.dumps(results))
