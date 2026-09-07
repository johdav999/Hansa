import bpy,pathlib,json,math
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1];bpy.ops.wm.open_mainfile(filepath=str(P/'checkpoints/portable.blend'));S=bpy.context.scene;o=bpy.data.objects['SM_HansaTowerMill'];uv=o.data.uv_layers.active.data;o.data.calc_loop_triangles();stats={m.name:[] for m in o.data.materials}
for t in o.data.loop_triangles:
 a,b,c=[o.data.vertices[i].co for i in t.vertices];area=(b-a).cross(c-a).length*.5
 u,v,w=[uv[i].uv for i in t.loops];tex=abs((v.x-u.x)*(w.y-u.y)-(v.y-u.y)*(w.x-u.x))*.5
 if area>1e-4 and tex>1e-9:stats[o.data.materials[t.material_index].name].append((1024*math.sqrt(tex/area),area))
result={}
for n,vals in stats.items():
 vals.sort();total=sum(a for d,a in vals)
 def quantile(q):
  s=0
  for d,a in vals:
   s+=a
   if s>=q*total:return d
 result[n]={'sampled_surface_m2':total,'area_weighted_p10_px_per_m':quantile(.1),'median_px_per_m':quantile(.5),'minimum_sampled_px_per_m':vals[0][0]}
(P/'exports/texel_density.json').write_text(json.dumps(result,indent=2))
for obj in S.objects:obj.hide_render=True
S.render.resolution_x=1000;S.render.resolution_y=1000;S.eevee.use_gtao=False;S.camera=bpy.data.objects['Hero'];S.camera.location=(0,-3.3,2.8);S.camera.rotation_euler=(Vector((0,0,0))-S.camera.location).to_track_quat('-Z','Y').to_euler();S.camera.data.lens=50
light=bpy.data.objects['Daylight'];light.hide_render=False;light.location=(-1,-2,4);light.rotation_euler=(Vector((0,0,0))-light.location).to_track_quat('-Z','Y').to_euler();light.data.energy=150;light.data.size=3
bpy.ops.mesh.primitive_plane_add(size=2);plane=bpy.context.object
for lp in plane.data.uv_layers.active.data:lp.uv*=2
vc=plane.data.vertex_colors.new(name='Weathering')
for v in vc.data:v.color=(1,1,1,1)
for name in ['Masonry','WeatheredTimber','SagePaint','Fieldstone']:
 plane.data.materials.clear();plane.data.materials.append(bpy.data.materials[name]);S.render.filepath=str(P/'renders'/('swatch_'+name+'.png'));bpy.ops.render.render(write_still=True);S.render.image_settings.file_format='JPEG';bpy.data.images['Render Result'].save_render(str(P/'renders'/('swatch_'+name+'.jpg')),scene=S);S.render.image_settings.file_format='PNG'
print('SWATCHES_AND_DENSITY_MEASURED',flush=True)
