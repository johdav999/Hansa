import bpy,pathlib
P=pathlib.Path(__file__).resolve().parents[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(str(P/'checkpoints/tower_r1.blend'),link=False) as (src,dst):dst.collections=src.collections;dst.worlds=src.worlds
print('LOADED_UNLINKED',flush=True)
for me in bpy.data.meshes:
 changed=me.validate(verbose=True,clean_customdata=True)
 if changed:print('REPAIRED',me.name,flush=True)
 me.update()
print('VALIDATED',flush=True)
S=bpy.context.scene
for c in dst.collections:S.collection.children.link(c)
S.world=bpy.data.worlds.get('Overcast daylight');S.camera=bpy.data.objects['Front'];S.unit_settings.system='METRIC';S.render.engine='BLENDER_EEVEE';S.eevee.use_gtao=True;S.eevee.gtao_distance=1;S.eevee.taa_render_samples=48;S.render.resolution_x=1000;S.render.resolution_y=1200;S.render.resolution_percentage=100;S.view_settings.view_transform='Filmic';S.view_settings.look='Medium High Contrast'
bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints/tower_r1_repaired.blend'));print('SAVED_REPAIRED',flush=True)
