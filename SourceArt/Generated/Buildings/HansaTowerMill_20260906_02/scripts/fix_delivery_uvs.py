import bpy,pathlib
P=pathlib.Path(__file__).resolve().parents[1];bpy.ops.wm.open_mainfile(filepath=str(P/'checkpoints/tower_r4.blend'))
for col in bpy.context.scene.collection.children:
 if col.name=='Review':continue
 for o in col.objects:
  if o.type!='MESH':continue
  me=o.data
  if not me.uv_layers:me.uv_layers.new(name='SurfaceMetres')
  preferred=me.uv_layers.get('SurfaceMetres') or me.uv_layers.active
  for uv in list(me.uv_layers):
   if uv!=preferred:me.uv_layers.remove(uv)
  me.uv_layers[0].name='SurfaceMetres';uv=me.uv_layers[0]
  stone=o.name.startswith('Exposed_fieldstone');brick=o.name.startswith('Brick_');tower=o.name=='Tapered_masonry_tower'
  if stone or brick or tower:
   for f in me.polygons:
    # Cylindrical tower face UVs stay intact; reveal floors/ceilings/jambs need planar metre coordinates.
    center=o.matrix_world@f.center;radial=(center.x*center.x+center.y*center.y)**.5
    outward=abs((f.normal.x*center.x+f.normal.y*center.y)/max(radial,.001))
    if tower and outward>.75:continue
    axis=max(range(3),key=lambda k:abs(f.normal[k]));axes=[k for k in range(3) if k!=axis];scale=2 if brick else .4 if tower else 1
    for li in f.loop_indices:
     v=me.vertices[me.loops[li].vertex_index].co;uv.data[li].uv=(v[axes[0]]*scale,v[axes[1]]*scale)
bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints/tower_r5.blend'));print('UV_CHANNELS_NORMALIZED')
