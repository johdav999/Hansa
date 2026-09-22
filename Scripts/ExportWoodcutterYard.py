"""Bake the reviewed hybrid materials and export portable deliverables."""
import bpy, json, math
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
JOB=ROOT/'SourceArt/Generated/Buildings/HansaWoodcutterYard_20260916'
OUT=JOB/'delivery';OUT.mkdir(exist_ok=True)
TEX=OUT/'textures';TEX.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(JOB/'review-r04/woodcutter-yard.blend'))
scene=bpy.context.scene;scene.cycles.samples=16
model=[o for o in scene.objects if o.type=='MESH' and o.name!='PreviewGround']
materials=list({m.name:m for o in model for m in o.data.materials}.values())
manifest=[]
for material in materials:
    bpy.ops.object.select_all(action='DESELECT')
    bpy.ops.mesh.primitive_plane_add(size=1,location=(0,0,-10));plane=bpy.context.object
    plane.data.materials.append(material)
    p=material.node_tree.nodes.get('Principled BSDF')
    maps={}
    for kind in ['BaseColor','Roughness','Normal']:
        size=1254 if kind=='BaseColor' and any(n.type=='TEX_IMAGE' for n in material.node_tree.nodes) else 1024
        im=bpy.data.images.new(material.name+'_'+kind,width=size,height=size,alpha=False)
        im.colorspace_settings.name='sRGB' if kind=='BaseColor' else 'Non-Color'
        target=material.node_tree.nodes.new('ShaderNodeTexImage');target.image=im;material.node_tree.nodes.active=target
        scene.render.bake.use_pass_direct=False;scene.render.bake.use_pass_indirect=False;scene.render.bake.use_pass_color=True
        bpy.ops.object.bake(type={'BaseColor':'DIFFUSE','Roughness':'ROUGHNESS','Normal':'NORMAL'}[kind],margin=4)
        path=TEX/(material.name+'_'+kind+'.png');im.filepath_raw=str(path);im.file_format='PNG';im.save()
        maps[kind]=path.name
        material.node_tree.nodes.remove(target)
    portable=bpy.data.materials.new('Portable_'+material.name);portable.use_nodes=True
    pp=portable.node_tree.nodes.get('Principled BSDF');pp.inputs['Metallic'].default_value=p.inputs['Metallic'].default_value
    for kind,file in maps.items():
        tex=portable.node_tree.nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(TEX/file),check_existing=True)
        if kind!='BaseColor':tex.image.colorspace_settings.name='Non-Color'
        if kind=='Normal':
            norm=portable.node_tree.nodes.new('ShaderNodeNormalMap');portable.node_tree.links.new(tex.outputs['Color'],norm.inputs['Color']);portable.node_tree.links.new(norm.outputs[0],pp.inputs['Normal'])
        else:portable.node_tree.links.new(tex.outputs['Color'],pp.inputs['Base Color' if kind=='BaseColor' else 'Roughness'])
    for o in model:
        for slot in o.material_slots:
            if slot.material==material:slot.material=portable
    manifest.append(dict(name=portable.name,source=material.name,maps=maps,metallic=pp.inputs['Metallic'].default_value,normal='OpenGL +Y',physical_tile_metres=1))
    bpy.data.objects.remove(plane,do_unlink=True)
for im in bpy.data.images:
    if im.source=='FILE':im.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'woodcutter-yard.blend'))
# Apply geometry and join only model parts; retain editability in packed master above.
bpy.ops.object.select_all(action='DESELECT')
for o in model:o.select_set(True)
bpy.context.view_layer.objects.active=model[0]
bpy.ops.object.convert(target='MESH');bpy.ops.object.join();mesh=bpy.context.object;mesh.name='SM_WoodcutterYard'
scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
bpy.ops.export_scene.fbx(filepath=str(OUT/'SM_WoodcutterYard.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,path_mode='COPY',embed_textures=False)
bpy.ops.export_scene.gltf(filepath=str(OUT/'SM_WoodcutterYard.glb'),use_selection=True,export_format='GLB')
scene.camera=scene.objects['Review_Hero'];scene.render.filepath=str(OUT/'portable-hero.png');bpy.ops.render.render(write_still=True)
manifest_data={'materials':manifest,'source_texture_native':[1254,1254],'uv_density':'1254 texels/metre for wood base color, 1024 for relief','bounds_metres':list(mesh.dimensions),'vertices':len(mesh.data.vertices),'triangles':sum(len(p.vertices)-2 for p in mesh.data.polygons)}
(OUT/'manifest.json').write_text(json.dumps(manifest_data,indent=2))
# A real turntable, saved as native frames without resampling.
scene.render.resolution_x=800;scene.render.resolution_y=600;scene.cycles.samples=12
for i in range(12):
    angle=math.tau*i/12;c=scene.camera;c.location=(15*math.cos(angle),15*math.sin(angle),11);c.rotation_euler=(Vector((0,0,1.7))-c.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(OUT/f'turntable-{i:02d}.png');bpy.ops.render.render(write_still=True)
# Clean reimport each format, retaining only review lights and ground.
for fmt in ['fbx','glb']:
    bpy.ops.wm.open_mainfile(filepath=str(OUT/'woodcutter-yard.blend'))
    for o in list(bpy.context.scene.objects):
        if o.type=='MESH' and o.name!='PreviewGround':bpy.data.objects.remove(o,do_unlink=True)
    if fmt=='fbx':bpy.ops.import_scene.fbx(filepath=str(OUT/'SM_WoodcutterYard.fbx'))
    else:bpy.ops.import_scene.gltf(filepath=str(OUT/'SM_WoodcutterYard.glb'))
    s=bpy.context.scene;s.camera=s.objects['Review_Hero'];s.render.filepath=str(OUT/f'reimport-{fmt}.png');bpy.ops.render.render(write_still=True)
    imported=[o for o in s.objects if o.type=='MESH' and o.name!='PreviewGround']
    assert imported and all(o.data.uv_layers for o in imported)
    (OUT/f'reimport-{fmt}.json').write_text(json.dumps([{'name':o.name,'dimensions':list(o.dimensions),'materials':[m.name for m in o.data.materials]} for o in imported],indent=2))
print('EXPORT_AND_REIMPORT_COMPLETE')
