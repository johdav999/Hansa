from pathlib import Path
P=Path(__file__).resolve().parents[1];p=P/'scripts/build_tower.py';s=p.read_text()
s=s.replace("bpy.context.view_layer.objects.active=o;mod=o.modifiers.new('True masonry reveal'", "bpy.context.view_layer.objects.active=c;c.select_set(True);bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.normals_make_consistent(inside=False);bpy.ops.object.mode_set(mode='OBJECT');bpy.context.view_layer.objects.active=o;mod=o.modifiers.new('True masonry reveal'")
s=s.replace("uv=tower.data.uv_layers.new(name='SurfaceMetres')", "tower.data.uv_layers.clear();uv=tower.data.uv_layers.new(name='SurfaceMetres')")
s=s.replace("tone=random.uniform(.78,1.05) if name=='Roof' else random.uniform(.88,1.0)", "tone=random.uniform(.51,.78) if name=='Roof' else random.uniform(.62,.80) if name=='Sails' else random.uniform(.82,1.0)")
p.write_text(s)
