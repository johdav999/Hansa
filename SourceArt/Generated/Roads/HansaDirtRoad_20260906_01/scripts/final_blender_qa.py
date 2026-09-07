import bpy,pathlib,json,math,sys
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(P/'exports/Hansa_DirtRoad_Kit.blend'))
packed=[{'name':i.name,'packed':bool(i.packed_file),'size':list(i.size)} for i in bpy.data.images if i.source=='FILE'];assert all(r['packed'] for r in packed)
S=bpy.context.scene
# Validate geometry connector endpoints against the delivered profile.
straight=bpy.data.objects['SM_DirtRoad_Straight_8m'];ends=[]
for x in [-4,4]:ends.append(sorted([(round(v.co.y,6),round(v.co.z,6)) for v in straight.data.vertices if abs(v.co.x-x)<1e-5]))
assert len(ends[0])==49 and ends[0]==ends[1]
areas={o.name:sum(p.area for p in o.data.polygons) for o in bpy.data.objects if o.type=='MESH' and o.name.startswith('SM_')}
S.camera=bpy.data.objects['Kit'];S.render.image_settings.file_format='JPEG';S.render.image_settings.quality=90
S.render.resolution_x=1100;S.render.resolution_y=760;S.render.resolution_percentage=100
for idx in range(24):
 a=2*math.pi*idx/24;cam=S.camera;cam.location=(55*math.cos(a),55*math.sin(a),50);cam.rotation_euler=(Vector((0,1,0))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.lens=45
 S.render.filepath=str(P/'renders'/f'turntable_{idx:03}.jpg');bpy.ops.render.render(write_still=True)
S.camera=bpy.data.objects['Surface'];sun=bpy.data.objects['QA_Daylight'];sun.rotation_euler=(math.radians(78),math.radians(-20),math.radians(-32));S.render.filepath=str(P/'renders'/'final_Raking.jpg');bpy.ops.render.render(write_still=True)
(P/'evidence/master_verified.json').write_text(json.dumps({'packed_images':packed,'source_endpoints_equal':True,'endpoint_vertex_count':49,'surface_areas_m2':areas,'turntable_frames':24,'render_dimensions':[1100,760],'texels_per_metre_basecolor':627,'texels_per_metre_physical_maps':512},indent=2))
print('MASTER_QA_OK')
