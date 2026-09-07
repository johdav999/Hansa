from pathlib import Path
p=Path(__file__).with_name('build_mill.py');s=p.read_text()
s=s.replace("  o.rotation_euler[2]=a+math.pi/2", """  o.rotation_euler[2]=a+math.pi/2
  if REV>=3:
   loc=o.location.copy();bpy.data.objects.remove(o,do_unlink=True)
   bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2,radius=1,location=loc);o=bpy.context.object;o.name=f'Fieldstone_{row}_{i}';o.scale=(w*.64,.38,h*.65)
   bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
   for v in o.data.vertices:v.co+=Vector([random.uniform(-.019,.019) for _ in range(3)])
   o.rotation_euler[2]=a+math.pi/2;o.data.materials.append(stone);uvmap(o);move(o,'Stonework')
   for f in o.data.polygons:f.use_smooth=True""")
s=s.replace("1.74,1.1,stone", "1.94 if REV>=3 else 1.74,1.1,stone")
p.write_text(s)
