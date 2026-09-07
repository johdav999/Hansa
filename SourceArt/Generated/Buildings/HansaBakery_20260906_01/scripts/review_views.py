import bpy,pathlib
P=pathlib.Path(__file__).resolve().parents[1];S=bpy.context.scene
for name in ['Detail_Shop','Detail_Gable','Detail_Roof','Rear']:
 S.camera=bpy.data.objects[name];S.render.filepath=str(P/'renders'/('r3_'+name+'.png'));bpy.ops.render.render(write_still=True)
