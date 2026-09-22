import bpy,json,sys,math
from pathlib import Path
from mathutils import Vector
J=Path(__file__).resolve().parents[1];E=J/'exports';fmt=sys.argv[sys.argv.index('--')+1]
bpy.ops.wm.open_mainfile(filepath=str(E/'HansaMaltHouse_Portable.blend'));S=bpy.context.scene
for o in list(bpy.data.objects):
 if not any(c.name=='Review' for c in o.users_collection):bpy.data.objects.remove(o,do_unlink=True)
for m in list(bpy.data.materials):
 if m.name!='ReviewGround':bpy.data.materials.remove(m,do_unlink=True)
for im in list(bpy.data.images):
 if im.name not in ['Render Result','Viewer Node']:bpy.data.images.remove(im)
if fmt=='fbx':bpy.ops.import_scene.fbx(filepath=str(E/'SM_HansaMaltHouse.fbx'))
else:bpy.ops.import_scene.gltf(filepath=str(E/'SM_HansaMaltHouse.glb'))
parts=[o for o in bpy.data.objects if o.type=='MESH' and not any(c.name=='Review' for c in o.users_collection)]
pts=[o.matrix_world@v.co for o in parts for v in o.data.vertices];lo=[min(v[a] for v in pts) for a in range(3)];hi=[max(v[a] for v in pts) for a in range(3)]
audit={'format':fmt,'bounds_min':lo,'bounds_max':hi,'size':[hi[i]-lo[i] for i in range(3)],'materials':sorted(set(s.material.name for o in parts for s in o.material_slots if s.material)),'images':[{'name':i.name,'size':list(i.size),'packed':bool(i.packed_file)} for i in bpy.data.images if i.name not in ['Render Result','Viewer Node']]}
expected=json.loads((E/'geometry-audit.json').read_text())['size_m'];assert all(abs(a-b)<.005 for a,b in zip(audit['size'],expected));assert len(audit['materials'])==7
for im in bpy.data.images:
 if im.name not in ['Render Result','Viewer Node']:assert im.size[0]>0
(E/('clean-'+fmt+'.json')).write_text(json.dumps(audit,indent=2))
S.render.image_settings.file_format='PNG';S.render.filepath=str(J/'renders'/('clean-'+fmt+'.png'));bpy.ops.render.render(write_still=True);S.render.image_settings.file_format='JPEG';S.render.image_settings.quality=93;bpy.data.images['Render Result'].save_render(str(J/'renders'/('clean-'+fmt+'.jpg')),scene=S)
S.camera.location=(12,-8,5);S.camera.rotation_euler=(Vector((4,-1.5,2.2))-S.camera.location).to_track_quat('-Z','Y').to_euler();S.camera.data.ortho_scale=4.7;S.render.image_settings.file_format='PNG';S.render.filepath=str(J/'renders'/('clean-'+fmt+'-detail.png'));bpy.ops.render.render(write_still=True);S.render.image_settings.file_format='JPEG';S.render.image_settings.quality=93;bpy.data.images['Render Result'].save_render(str(J/'renders'/('clean-'+fmt+'-detail.jpg')),scene=S)
print('CLEAN_IMPORT_PASS',fmt,json.dumps(audit))
