import bpy,pathlib,json
P=pathlib.Path(__file__).resolve().parents[1];inv=json.loads((P/'material_inventory.json').read_text());bpy.ops.wm.open_mainfile(filepath=str(P/'exports/HansaMill.blend'));S=bpy.context.scene
for o in S.objects:o.hide_render=True
bpy.ops.mesh.primitive_plane_add(size=1);plane=bpy.context.object;S.render.engine='CYCLES';S.cycles.samples=8;S.render.bake.margin=0
for rec in inv:
 m=bpy.data.materials[rec['name']].copy();plane.data.materials.clear();plane.data.materials.append(m);n=m.node_tree.nodes;l=m.node_tree.links;out=n.get('Material Output');w=n.get('Geometry_bound_weathering');em=n.new('ShaderNodeEmission')
 if w.inputs[1].links:l.new(w.inputs[1].links[0].from_socket,em.inputs['Color'])
 else:em.inputs['Color'].default_value=w.inputs[1].default_value
 l.new(em.outputs[0],out.inputs[0]);im=bpy.data.images.new('Native1024_'+rec['name'],width=1024,height=1024,alpha=False);im.colorspace_settings.name='sRGB';t=n.new('ShaderNodeTexImage');t.image=im;n.active=t;bpy.ops.object.bake(type='EMIT');path=P/'exports'/('T_Mill_'+rec['name']+'_BaseColor_1024_v2.png');im.filepath_raw=str(path);im.file_format='PNG';im.save();rec['previous_base_color']=rec['maps']['BaseColor'];rec['maps']['BaseColor']=str(path);rec['base_color_px_per_m']=1024;rec['native_bake_note']='1024 native shader bake for mipmaps; original 1254 ImageGen input unmodified';bpy.data.materials.remove(m)
(P/'material_inventory.json').write_text(json.dumps(inv,indent=2))
bpy.ops.wm.open_mainfile(filepath=str(P/'checkpoints/portable.blend'));S=bpy.context.scene;o=bpy.data.objects['SM_HansaMill'];bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
for rec in inv:
 m=bpy.data.materials[rec['name']];n=m.node_tree.nodes;l=m.node_tree.links;b=n.get('Principled BSDF');image=next(t for t in n if t.type=='TEX_IMAGE' and 'BaseColor' in t.image.name);image.image=bpy.data.images.load(rec['maps']['BaseColor'],check_existing=True);l.new(image.outputs['Color'],b.inputs['Base Color'])
bpy.ops.export_scene.gltf(filepath=str(P/'exports/HansaMill.glb'),export_format='GLB',use_selection=True,export_colors=True)
bpy.ops.export_scene.fbx(filepath=str(P/'exports/HansaMill.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Z',axis_up='Y',apply_unit_scale=True,path_mode='COPY',embed_textures=False,add_leaf_bones=False)
for m in o.data.materials:
 n=m.node_tree.nodes;l=m.node_tree.links;mix=next(t for t in n if t.type=='MIX_RGB');l.new(mix.outputs[0],n.get('Principled BSDF').inputs['Base Color'])
bpy.ops.wm.save_as_mainfile(filepath=str(P/'checkpoints/portable_1024.blend'));S.camera=bpy.data.objects['Hero'];S.render.filepath=str(P/'renders/portable_1024_Hero.png');bpy.ops.render.render(write_still=True);print('NATIVE_MIP_VARIANT_BAKED',flush=True)
