import bpy,json,math
from pathlib import Path
P=Path('C:/Users/Johan/source/repos/Unreal/Hansa/Hansa/SourceArt/Buildings/hansa-malthouse')
bpy.ops.wm.open_mainfile(filepath=str(P/'Model/HansaMaltHouse.blend'))
images=[{'name':i.name,'size':list(i.size),'packed':bool(i.packed_file)} for i in bpy.data.images if i.source=='FILE']
assert all(i['packed'] and i['size'][0]>0 for i in images)
dens=[]
for o in bpy.data.objects:
 if o.type!='MESH' or any(c.name=='Review' for c in o.users_collection) or not o.data.uv_layers.active:continue
 m=o.data;m.calc_loop_triangles();uv=m.uv_layers.active.data
 for t in m.loop_triangles:
  a,b,c=[o.matrix_world@m.vertices[v].co for v in t.vertices]
  area=(b-a).cross(c-a).length/2
  p,q,r=[uv[l].uv for l in t.loops];ua=abs((q.x-p.x)*(r.y-p.y)-(q.y-p.y)*(r.x-p.x))/2
  if area>1e-8 and ua>1e-12:dens.append(1024*math.sqrt(ua/area))
dens.sort()
report={'delivered_master_reopened':True,'all_file_images_packed':True,'images':images,'texel_density_area_equivalent_px_m':{'min':dens[0],'p05':dens[int(len(dens)*.05)],'median':dens[len(dens)//2],'max':dens[-1]},'note':'Triangle area-equivalent UV density; projected mappings are anisotropic on sloped surfaces.'}
(P/'Evidence/source-reopen-audit.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report))
