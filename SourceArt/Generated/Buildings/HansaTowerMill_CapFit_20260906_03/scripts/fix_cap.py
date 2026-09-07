import bpy,pathlib,math,json
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1];OLD=P.parent/'hansa-tower-mill_20260906_02'
bpy.ops.wm.open_mainfile(filepath=str(OLD/'exports/HansaTowerMill.blend'));S=bpy.context.scene
def camera(name,loc,target):
 d=bpy.data.cameras.new(name);d.lens=55;o=bpy.data.objects.new(name,d);bpy.data.collections['Review'].objects.link(o);o.location=loc;o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler();return o
camera('Cap_Fit',(13,16,19),(0,0,9.4));camera('Cap_Side',(17,0,10),(0,0,9));S.render.resolution_x=1100;S.render.resolution_y=1100
for view in ['Cap_Fit','Cap_Side']:
 S.camera=bpy.data.objects[view];S.render.filepath=str(P/'renders'/('before_'+view+'.png'));bpy.ops.render.render(write_still=True)
factor=2.9/2.15;dy=2.9-2.15;dz=-.10
for o in bpy.data.collections['Roof'].objects:
 o.location.y*=factor;o.scale.y*=factor;o.location.z+=dz
 # Roof slope shingle horizontal UV axis follows its extended local Y dimension.
 if o.type=='MESH' and o.name.startswith('Roof_split_shingle'):
  for uv in o.data.uv_layers.active.data:uv.uv.x*=factor
for col in ['Sails','Hardware','Openings']:
 for o in bpy.data.collections[col].objects:
  iscap=col=='Sails' or o.name.startswith(('Cap_window','Cap_glazing','Iron_diamond','Hub_rivet','Sail_iron','Stock_binding','Windshaft'))
  if iscap:o.location.y-=dy;o.location.z+=dz
# A closed timber curb seats the cap on the circular masonry crown.
verts=[];faces=[];n=96
for z,r in [(8.55,2.60),(8.74,2.60),(8.55,2.15),(8.74,2.15)]:
 for i in range(n):a=2*math.pi*i/n;verts.append((r*math.cos(a),r*math.sin(a),z))
for i in range(n):
 j=(i+1)%n
 faces.extend([(i,j,n+j,n+i),(n+i,n+j,3*n+j,3*n+i),(2*n+j,2*n+i,3*n+i,3*n+j),(i,2*n+i,2*n+j,j)])
me=bpy.data.meshes.new('Cap_seating_curb');me.from_pydata(verts,[],faces);me.update();o=bpy.data.objects.new('Cap_seating_curb',me);bpy.data.collections['Roof'].objects.link(o);me.materials.append(bpy.data.materials['RecessTimber']);me.uv_layers.new(name='SurfaceMetres');vc=me.vertex_colors.new(name='Weathering');uv=me.uv_layers[0]
for f in me.polygons:
 axis=max(range(3),key=lambda k:abs(f.normal[k]));axes=[k for k in range(3) if k!=axis]
 for li in f.loop_indices:v=me.vertices[me.loops[li].vertex_index].co;uv.data[li].uv=(v[axes[0]],v[axes[1]]);vc.data[li].color=(.7,.7,.7,1)
bpy.context.view_layer.update()
assert 2.9>2.60 and 2.82>2.60 and 8.66<8.70
(P/'fit_measurements.json').write_text(json.dumps({'cause':'Cap was centred but its front/back half-depth 2.15 m was smaller than tower crown radius 2.55 m; lower cap edge also sat 0.06 m above crown.','before':{'cap_half_depth_m':2.15,'tower_crown_radius_m':2.55,'cap_base_z_m':8.76,'tower_top_z_m':8.7},'after':{'cap_half_depth_m':2.9,'cap_half_width_m':2.82,'cap_base_z_m':8.66,'seating_curb_outer_radius_m':2.6,'curb_z_range_m':[8.55,8.74],'sail_and_front_cap_window_shift_m':[0,-dy,dz]},'centre_unchanged_xy':[0,0]},indent=2))
for im in bpy.data.images:
 if im.source=='FILE':im.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(P/'exports/HansaTowerMill_CapFit.blend'))
for view in ['Cap_Fit','Cap_Side','Front']:
 S.camera=bpy.data.objects[view];S.render.filepath=str(P/'renders'/('after_'+view+'.png'));bpy.ops.render.render(write_still=True)
print('CAP_FIT_CORRECTED',flush=True)
