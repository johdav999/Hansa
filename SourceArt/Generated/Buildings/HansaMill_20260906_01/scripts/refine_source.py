from pathlib import Path
p=Path(__file__).with_name('build_mill.py');s=p.read_text()
s=s.replace(".15+row*.25),(w,.62,h),stone,'Stonework',.06)",".15+row*.25+(random.uniform(-.035,.035) if REV>=1 else 0)),(w,.62,h),stone,'Stonework',.045 if REV>=1 else .06)")
s=s.replace("  for j,(a,b) in enumerate(intervals):box(f'End_plank_{y}_{i}_{j}',(x,y+random.uniform(-.007,.007),(a+b)/2),(.172,.065,b-a),oak,'Cladding',.005)","""  for j,(a,b) in enumerate(intervals):
   o=box(f'End_plank_{y}_{i}_{j}',(x,y+random.uniform(-.007,.007),(a+b)/2),(.172,.065,b-a),oak,'Cladding',.005)
   if REV>=1 and b==top:
    for v in o.data.vertices:
     if v.co.z>0:v.co.z=eave+(ridge-eave)*(1-abs(x+v.co.x)/1.8)-o.location.z""")
s=s.replace("for sign in [-1,1]:\n for row", """for sign in [-1,1]:
 if REV>=1:
  under=box('Roof_underboarding',(sign*.99,0,(eave+ridge)/2-.035),(length,4.6,.05),dark,'Roof',.003,0);under.rotation_euler[1]=sign*slope
 for row""")
s=s.replace('# Neutral review stage, independent from exported mesh.',"""# Editable geometry-aware broad weathering. No photo luminance is converted to height.
if REV>=1:
 for c in collections.values():
  for o in c.objects:
   if o.type!='MESH':continue
   vc=o.data.vertex_colors.new(name='Weathering');tone=random.uniform(.70,1.02) if c.name=='Stonework' else random.uniform(.83,1.02)
   for f in o.data.polygons:
    for li in f.loop_indices:
     v=o.matrix_world@o.data.vertices[o.data.loops[li].vertex_index].co
     patch=.5+.5*math.sin(v.x*4.7+math.sin(v.y*6.1))*math.sin(v.y*3.8+.7)
     damp=max(0,1-v.z/1.25)*patch*.34 if c.name=='Stonework' else max(0,1-abs(v.z-1.67)/.35)*patch*.28
     t=tone*(1-damp);vc.data[li].color=(t,t*(1-.02*damp),t*(1-.11*damp),1)
 for mat in [oak,roof,stone,dark,iron]:
  n=mat.node_tree.nodes;l=mat.node_tree.links;b=n.get('Principled BSDF');incoming=list(b.inputs['Base Color'].links)
  vc=n.new('ShaderNodeVertexColor');vc.layer_name='Weathering';mix=n.new('ShaderNodeMixRGB');mix.name='Geometry_bound_weathering';mix.blend_type='MULTIPLY';mix.inputs[0].default_value=1
  if incoming:l.new(incoming[0].from_socket,mix.inputs[1])
  else:mix.inputs[1].default_value=b.inputs['Base Color'].default_value
  l.new(vc.outputs['Color'],mix.inputs[2]);l.new(mix.outputs[0],b.inputs['Base Color'])
# Neutral review stage, independent from exported mesh.""")
p.write_text(s)
