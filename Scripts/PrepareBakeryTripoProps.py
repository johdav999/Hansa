"""Normalize user-supplied Tripo FBXs; retain texture pixels and source hashes."""
import bpy, json, hashlib, math
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceArt/Generated/Props/BakeryTripo_20260922'
OUT.mkdir(parents=True, exist_ok=True)
SPECS = [('BreadRack', 'bread rack', 'breadrack.fbx', 1.8),
         ('FirewoodBasket', 'firewood basket', 'firewood basket.fbx', .75),
         ('Handcart', 'handcart', 'handcart.fbx', 2.2),
         ('Millstone', 'millstone', 'millstone.fbx', 1.3)]
records = []
for name, folder, filename, extent in SPECS:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    source = ROOT / 'Content/Mesh/hansa-bakery' / folder / filename
    bpy.ops.import_scene.fbx(filepath=str(source))
    objects = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    for o in objects: o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = 'SM_Bakery_' + name
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    points = [v.co.copy() for v in obj.data.vertices]
    lo = Vector([min(p[i] for p in points) for i in range(3)])
    hi = Vector([max(p[i] for p in points) for i in range(3)])
    original = list(hi - lo)
    scale = extent / max(hi - lo)
    center = Vector(((lo.x+hi.x)/2, (lo.y+hi.y)/2, lo.z))
    for v in obj.data.vertices: v.co = (v.co-center)*scale
    maps = {}
    for kind in ('basecolor','roughness','metallic','normal'):
        maps[kind] = next(source.parent.rglob('*_'+kind+'.*'))
    mat = bpy.data.materials.new('M_Bakery_' + name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    for kind, path in maps.items():
        tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
        tex.image = bpy.data.images.load(str(path), check_existing=False)
        if kind != 'basecolor': tex.image.colorspace_settings.name = 'Non-Color'
        if kind == 'normal':
            normal = mat.node_tree.nodes.new('ShaderNodeNormalMap')
            mat.node_tree.links.new(tex.outputs['Color'], normal.inputs['Color'])
            mat.node_tree.links.new(normal.outputs['Normal'], bsdf.inputs['Normal'])
        else:
            mat.node_tree.links.new(tex.outputs['Color'], bsdf.inputs[{'basecolor':'Base Color','roughness':'Roughness','metallic':'Metallic'}[kind]])
    obj.data.materials.clear(); obj.data.materials.append(mat)
    for poly in obj.data.polygons: poly.material_index = 0
    triangles_before = sum(len(p.vertices)-2 for p in obj.data.polygons)
    if triangles_before > 18000:
        mod = obj.modifiers.new('StrategyCameraReduction', 'DECIMATE')
        mod.ratio = 18000/triangles_before
        bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.scale_length = 1
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(name+'.blend')))
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(name+'.blend')))
    bpy.ops.export_scene.fbx(filepath=str(OUT/(obj.name+'.fbx')), use_selection=True,
        object_types={'MESH'}, axis_forward='-Y', axis_up='Z', apply_unit_scale=True,
        bake_space_transform=False, path_mode='COPY', embed_textures=False)
    record = dict(name=name, source=str(source.relative_to(ROOT)), sourceSha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        originalBounds=original, scale=scale, bounds=list(obj.dimensions), trianglesBefore=triangles_before,
        triangles=sum(len(p.vertices)-2 for p in obj.data.polygons), maps={k:str(v.relative_to(ROOT)) for k,v in maps.items()},
        textureDimensions={k:list(next(n.image.size for n in mat.node_tree.nodes if n.type=='TEX_IMAGE' and Path(n.image.filepath).name==p.name)) for k,p in maps.items()})
    records.append(record)
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE'
    scene.eevee.use_gtao = True
    scene.world = bpy.data.worlds.new('Neutral')
    scene.world.use_nodes = True
    scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.32,.32,.32,1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value = .7
    bpy.ops.object.light_add(type='AREA', location=(3,-4,6)); bpy.context.object.data.energy=500; bpy.context.object.data.size=5
    target=Vector((0,0,obj.dimensions.z*.45))
    bpy.ops.object.camera_add(location=(extent*1.6,-extent*2,extent*1.4))
    camera=bpy.context.object; camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.type='ORTHO'; camera.data.ortho_scale=extent*1.65; scene.camera=camera
    scene.render.resolution_x=1000; scene.render.resolution_y=1000; scene.render.resolution_percentage=100
    scene.view_settings.view_transform='Standard'; scene.view_settings.look='Medium High Contrast'
    scene.render.filepath=str(OUT/(name+'-review.png')); bpy.ops.render.render(write_still=True)
    print('BAKERY_PROP',json.dumps(record),flush=True)
(OUT/'manifest.json').write_text(json.dumps(records,indent=2)+'\n')
