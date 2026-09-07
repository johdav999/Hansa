import bpy,pathlib,json,math
from mathutils import Vector,Matrix
from mathutils.bvhtree import BVHTree
P=pathlib.Path(__file__).resolve().parents[1];bpy.ops.wm.open_mainfile(filepath=str(P/'exports/HansaWindmill_Animated.blend'));S=bpy.context.scene;S.frame_set(1);rig=bpy.data.objects['Windmill_Rotor_Pivot'];images=[{'name':im.name,'packed':bool(im.packed_file),'dimensions':list(im.size)} for im in bpy.data.images if im.source=='FILE'];assert len(images)==5 and all(i['packed'] for i in images)
rotor=[o for o in S.objects if o.parent==rig and o.type=='MESH'];body=[o for c in S.collection.children if c.name!='Review' for o in c.objects if o.type=='MESH' and o not in rotor]
def geometry(objects,local=False):
 verts=[];polys=[];dep=bpy.context.evaluated_depsgraph_get()
 for o in objects:
  ev=o.evaluated_get(dep);me=ev.to_mesh();offset=len(verts);verts.extend([o.matrix_world@v.co-(rig.location if local else Vector((0,0,0))) for v in me.vertices]);polys.extend([tuple(offset+i for i in f.vertices) for f in me.polygons]);ev.to_mesh_clear()
 return verts,polys
bv,bp=geometry(body);rv,rp=geometry([o for o in rotor if o.name!='Windshaft'],True);fixed=BVHTree.FromPolygons(bv,bp);checks=[]
for degrees in range(0,360,5):
 rotation=Matrix.Rotation(math.radians(degrees),3,'Y');v=[rotation@p+rig.location for p in rv];overlaps=fixed.overlap(BVHTree.FromPolygons(v,rp));checks.append({'angle_degrees':degrees,'triangle_overlaps':len(overlaps)})
assert all(c['triangle_overlaps']==0 for c in checks),[c for c in checks if c['triangle_overlaps']]
result={'packed_images':images,'rotating_source_objects':len(rotor),'pivot_m':list(rig.location),'sweep_samples':checks,'shaft_exclusion':'Windshaft intentionally enters the cap bearing; all other moving geometry tested against stationary building.','surface_collision_checks_passed':True};(P/'master_verification.json').write_text(json.dumps(result,indent=2));print('MASTER_AND_SWEPT_CLEARANCE_VERIFIED')
