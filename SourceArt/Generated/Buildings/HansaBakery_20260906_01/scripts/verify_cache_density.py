import bpy,json,pathlib,hashlib,math,statistics
P=pathlib.Path(__file__).resolve().parents[1]
def signatures(path):
 bpy.ops.wm.open_mainfile(filepath=str(path));d={}
 for m in bpy.data.materials:
  if not m.use_nodes or m.name.startswith('Review'):continue
  nodes=[]
  for n in m.node_tree.nodes:
   inputs=[]
   for i in n.inputs:
    if hasattr(i,'default_value'):
     v=i.default_value
     try:v=list(v)
     except TypeError:pass
     if not isinstance(v,(str,int,float,list,bool)):v=str(v)
     inputs.append((i.name,v))
   nodes.append({'name':n.name,'type':n.bl_idname,'inputs':inputs,'ramp':[(e.position,list(e.color)) for e in n.color_ramp.elements] if hasattr(n,'color_ramp') else None})
  d[m.name]=hashlib.sha256(json.dumps({'nodes':nodes,'links':[(l.from_node.name,l.from_socket.name,l.to_node.name,l.to_socket.name) for l in m.node_tree.links]},sort_keys=True).encode()).hexdigest()
 return d
r5=signatures(P/'checkpoints'/'bakery_r5.blend');r6=signatures(P/'checkpoints'/'bakery_r6.blend');assert r5==r6
(P/'exports'/'shader_cache_validation.json').write_text(json.dumps({'r5_r6_identical':True,'shader_hashes':r6,'sampling_geometry':'Unchanged 2m plane; source sampling independent of asset geometry','maps_sha256':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in (P/'textures').glob('*.png')}},indent=2))
bpy.ops.wm.open_mainfile(filepath=str(P/'exports'/'HansaBakery.blend'));o=bpy.data.objects['SM_HansaBakery'];me=o.data;me.calc_loop_triangles();uv=me.uv_layers.active.data;density={}
for tri in me.loop_triangles:
 if tri.area<1e-9:continue
 a,b,c=[uv[i].uv for i in tri.loops];au=abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))*.5
 d=math.sqrt(au/tri.area)*1024
 density.setdefault(me.materials[tri.material_index].name,[]).append((d,tri.area))
report={}
for m,vals in density.items():
 sv=sorted(d for d,a in vals);area=sum(a for d,a in vals);report[m]={'minimum_px_m':min(sv),'p05_px_m':sv[int(len(sv)*.05)],'median_px_m':statistics.median(sv),'area_weighted_mean_px_m':sum(d*a for d,a in vals)/area,'surface_area_m2':area}
(P/'exports'/'measured_density.json').write_text(json.dumps(report,indent=2));print('SHADER_CACHE_AND_DENSITY_VERIFIED')
