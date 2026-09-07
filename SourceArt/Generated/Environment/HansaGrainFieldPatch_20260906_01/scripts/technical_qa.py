import bpy,json,math,numpy as np
from pathlib import Path
P=Path(__file__).resolve().parents[1];S=bpy.context.scene
ob=bpy.data.objects['SM_GrainFieldPatch_4m'];me=ob.data;me.calc_loop_triangles()
uv=np.array([l.uv[:] for l in me.uv_layers.active.data],dtype=np.float64)
co=np.array([v.co[:] for v in me.vertices],dtype=np.float64)
li=np.array([t.loops[:] for t in me.loop_triangles]);vi=np.array([t.vertices[:] for t in me.loop_triangles]);mi=np.array([t.material_index for t in me.loop_triangles])
v=co[vi];u=uv[li];a=np.linalg.norm(np.cross(v[:,1]-v[:,0],v[:,2]-v[:,0]),axis=1)/2
b=np.abs((u[:,1,0]-u[:,0,0])*(u[:,2,1]-u[:,0,1])-(u[:,1,1]-u[:,0,1])*(u[:,2,0]-u[:,0,0]))/2
d=1254*np.sqrt(b/np.maximum(a,1e-20));dens={}
for i,m in enumerate(me.materials):
 valid=(mi==i)&(a>1e-10)&(b>1e-10);dens[m.name]={'p05_px_per_m':float(np.quantile(d[valid],.05)),'median_px_per_m':float(np.median(d[valid])),'p95_px_per_m':float(np.quantile(d[valid],.95))}
images={im.name:{'size':list(im.size),'packed':bool(im.packed_file)} for im in bpy.data.images if im.source=='FILE'}
assert all(x['packed'] for x in images.values());assert np.isfinite(co).all()
report={'packed_master_reopened':True,'images':images,'texel_density':dens,'soil_density_px_per_m':1254,'finite_vertices':True,'triangles':len(me.loop_triangles),'zero_area_triangles':int(np.count_nonzero(a<1e-12))}
(P/'technical_qa.json').write_text(json.dumps(report,indent=2))
S.camera=bpy.data.objects['Review_Close'];sun=bpy.data.objects['Sun'];sun.rotation_euler=(math.radians(78),0,math.radians(-35));sun.data.energy=2.5
S.render.filepath=str(P/'renders'/'raking_close.jpg');bpy.ops.render.render(write_still=True)
print(json.dumps(report))
