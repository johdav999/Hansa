import bpy,pathlib
P=pathlib.Path(__file__).resolve().parents[1];S=bpy.context.scene
for o in bpy.data.objects:
 if o.name.startswith('Bakehouse_tile'):o.location.z+=.16
 if o.name=='Oven_chimney':o.dimensions.z=7.;o.location.z=6.85
 if o.name=='Chimney_flue':o.location.z=10.43
S.camera=bpy.data.objects['Hero'];bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints'/'bakery_r5.blend'))
for n in ['Rear','Detail_Shop']:
 S.camera=bpy.data.objects[n];S.render.filepath=str(P/'renders'/('r5_'+n+'.png'));bpy.ops.render.render(write_still=True)
