import bpy,pathlib,os,json,math
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1];fmt=os.environ.get('VERIFY_FORMAT','glb')
bpy.ops.wm.read_factory_settings(use_empty=True)
if fmt=='glb':bpy.ops.import_scene.gltf(filepath=str(P/'exports'/'HansaBakery.glb'))
else:bpy.ops.import_scene.fbx(filepath=str(P/'exports'/'HansaBakery.fbx'))
asset=[o for o in bpy.context.scene.objects if o.type=='MESH']
with bpy.data.libraries.load(str(P/'exports'/'HansaBakery.blend'),link=False) as (a,b):b.collections=['Review']
for c in b.collections:bpy.context.scene.collection.children.link(c)
S=bpy.context.scene;S.world=bpy.data.worlds.new('Neutral_daylight');S.world.use_nodes=True;bg=S.world.node_tree.nodes.get('Background');bg.inputs[0].default_value=(.65,.74,.85,1);bg.inputs[1].default_value=.6
S.render.engine='BLENDER_EEVEE';S.eevee.use_gtao=True;S.eevee.gtao_distance=3;S.eevee.gtao_factor=1.1;S.eevee.taa_render_samples=64;S.eevee.use_soft_shadows=True;S.render.resolution_x=1200;S.render.resolution_y=1200;S.render.resolution_percentage=100;S.view_settings.view_transform='Filmic';S.view_settings.look='Medium High Contrast'
S.render.image_settings.file_format='PNG'
points=[o.matrix_world@v.co for o in asset for v in o.data.vertices];bds=[min(p[k] for p in points) for k in range(3)]+[max(p[k] for p in points) for k in range(3)]
r={'format':fmt,'bounds_m':bds,'mesh_count':len(asset),'material_slots':[[m.name for m in o.data.materials if m] for o in asset],'images':[{'name':im.name,'size':list(im.size),'packed':bool(im.packed_file)} for im in bpy.data.images if im.type=='IMAGE']}
(P/'exports'/('reimport_'+fmt+'.json')).write_text(json.dumps(r,indent=2))
for view in ['Hero','Detail_Shop','Detail_Roof']:
 S.camera=bpy.data.objects[view];S.render.filepath=str(P/'renders'/('reimport_'+fmt+'_'+view+'.png'));bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints'/('reimport_'+fmt+'.blend')))
print('REIMPORT_CHECK',json.dumps({'format':fmt,'bounds':bds,'images':len(r['images'])}))
