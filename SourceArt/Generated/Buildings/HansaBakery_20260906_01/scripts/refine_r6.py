import bpy,bmesh,math,pathlib
P=pathlib.Path(__file__).resolve().parents[1];S=bpy.context.scene
for o in list(bpy.data.objects):
 if o.name.startswith(('Ridge_cap','Chimney_crown','Chimney_open_coping','Chimney_flue')):bpy.data.objects.remove(o,do_unlink=True)
def box(name,loc,dim,mat):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name=name;o.dimensions=dim;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 for c in list(o.users_collection):c.objects.unlink(o)
 bpy.data.collections['Masonry'].objects.link(o);o.data.materials.append(bpy.data.materials[mat]);b=o.modifiers.new('Masonry_edge','BEVEL');b.width=.01;b.segments=2
for j in range(50):
 y=-6.02+j*.257;v=[];f=[];N=13
 for radius in [.22,.18]:
  for yy in [y,y+.30]:
   for k in range(N):
    a=k*math.pi/(N-1);v.append((radius*math.cos(a),yy,14.40+radius*math.sin(a)))
 for k in range(N-1):
  f.extend([(k,k+1,N+k+1,N+k),(2*N+k,3*N+k,3*N+k+1,2*N+k+1),(k,2*N+k,2*N+k+1,k+1),(N+k,N+k+1,3*N+k+1,3*N+k)])
 f.extend([(0,N,3*N,2*N),(N-1,3*N-1,4*N-1,2*N-1)])
 me=bpy.data.meshes.new('Ridge_tile_shell');me.from_pydata(v,[],f);me.update();bm=bmesh.new();bm.from_mesh(me);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free();o=bpy.data.objects.new('Solid_overlapping_ridge_tile',me);bpy.data.collections['Roof'].objects.link(o);me.materials.append(bpy.data.materials['Clay_'+str(j%3)])
 for p in me.polygons:p.use_smooth=True
for z in [9.9,10.27]:box('Chimney_shoulder',(2.9,7.5,z),(1.01,1.01,.13),'Brick_1')
for x,y,dx,dy in [(2.9,7.04,1.08,.17),(2.9,7.96,1.08,.17),(2.44,7.5,.17,.78),(3.36,7.5,.17,.78)]:box('Hollow_chimney_coping',(x,y,10.87),(dx,dy,.19),'Brick_1')
box('Recessed_black_flue',(2.9,7.5,10.39),(.71,.71,.015),'Oven_Soot')
S.camera=bpy.data.objects['Hero'];bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints'/'bakery_r6.blend'))
for view in ['Detail_Roof','Rear']:
 S.camera=bpy.data.objects[view];S.render.filepath=str(P/'renders'/('r6_'+view+'.png'));bpy.ops.render.render(write_still=True)
