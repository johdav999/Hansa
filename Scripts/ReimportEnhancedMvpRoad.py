"""Clean-context road round trips and packed-source verification, no source resizing."""
import bpy,json,sys
from pathlib import Path
from mathutils import Vector
REPO=Path(__file__).resolve().parents[1];JOB=REPO/'Saved/GenerationJobs/hansa-road_P18_20260908';fmt=sys.argv[-1]
manifest=json.loads((JOB/'exports/export-manifest.json').read_text())
bpy.ops.wm.open_mainfile(filepath=str(JOB/'exports/HansaRoad.blend'))
assert all(i.packed_file for i in bpy.data.images if i.source=='FILE')
bpy.ops.wm.read_factory_settings(use_empty=True)
s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.samples=24;s.cycles.use_denoising=True
s.render.resolution_x=1280;s.render.resolution_y=720;s.render.resolution_percentage=100
s.view_settings.view_transform='Standard';s.view_settings.look='Medium High Contrast'
m=bpy.data.materials.new('PortableReload');m.use_nodes=True;n=m.node_tree.nodes;l=m.node_tree.links;bs=n.get('Principled BSDF')
for channel in ('BaseColor','Roughness','Normal'):
    im=bpy.data.images.load(str(JOB/'exports'/('M_Road_Earth_'+channel+'.png')));assert tuple(im.size)==(1024,1024)
    im.colorspace_settings.name='sRGB' if channel=='BaseColor' else 'Non-Color';tex=n.new('ShaderNodeTexImage');tex.image=im
    if channel=='Normal':
        nm=n.new('ShaderNodeNormalMap');l.new(tex.outputs['Color'],nm.inputs['Color']);l.new(nm.outputs[0],bs.inputs['Normal'])
    else:l.new(tex.outputs[0],bs.inputs['Base Color' if channel=='BaseColor' else 'Roughness'])
vc=n.new('ShaderNodeVertexColor');vc.layer_name='RoadBlend';l.new(vc.outputs['Alpha'],bs.inputs['Alpha']);m.blend_method='HASHED'
proof={}
for idx,(name,entry) in enumerate(manifest['modules'].items()):
    before=set(bpy.data.objects)
    if fmt=='fbx':bpy.ops.import_scene.fbx(filepath=str(JOB/'exports'/entry['fbx']))
    else:bpy.ops.import_scene.gltf(filepath=str(JOB/'exports'/entry['glb']))
    found=[o for o in set(bpy.data.objects)-before if o.type=='MESH' and not o.name.startswith('UCX_')]
    assert len(found)==1,(name,[o.name for o in found]);ob=found[0]
    bounds=[ob.matrix_world@v.co for v in ob.data.vertices]
    lo=[min(v[k] for v in bounds) for k in range(3)];hi=[max(v[k] for v in bounds) for k in range(3)]
    assert all(abs(lo[k]-entry['boundsMin'][k])<.02 and abs(hi[k]-entry['boundsMax'][k])<.02 for k in range(3)),(name,lo,hi)
    assert len(ob.data.uv_layers)>0 and len(ob.data.vertex_colors)>0,name
    for o in set(bpy.data.objects)-before:
        if o.name.startswith('UCX_'):bpy.data.objects.remove(o,do_unlink=True)
    if fmt=='fbx':ob.data.materials.clear();ob.data.materials.append(m)
    ob.location.x+=(idx%3-1)*5;ob.location.y+=(idx//3-.5)*5
    proof[name]={'boundsMin':lo,'boundsMax':hi,'uvLayers':len(ob.data.uv_layers),'colorLayers':len(ob.data.vertex_colors)}
s.world=bpy.data.worlds.new('Neutral');s.world.use_nodes=True;s.world.node_tree.nodes['Background'].inputs[0].default_value=(.65,.7,.76,1);s.world.node_tree.nodes['Background'].inputs[1].default_value=.65
bpy.ops.object.light_add(type='AREA',location=(0,-8,14));light=bpy.context.object;light.data.energy=1700;light.data.size=8;light.rotation_euler=(Vector((0,0,0))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.mesh.primitive_plane_add(size=60);ground=bpy.context.object;gm=bpy.data.materials.new('ReviewSoil');gm.diffuse_color=(.23,.25,.19,1);ground.data.materials.append(gm)
for tag,pos,target,lens in [('kit',(13,-18,20),(0,0,0),48),('detail',(4,-8,5),(0,-2.5,0),52)]:
    bpy.ops.object.camera_add(location=pos);cam=bpy.context.object;cam.data.lens=lens;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();s.camera=cam
    s.render.filepath=str(JOB/'renders'/f'reimport-{fmt}-{tag}.png');bpy.ops.render.render(write_still=True)
(JOB/'evidence'/f'reimport-{fmt}.json').write_text(json.dumps(proof,indent=2))
print('P18_REIMPORT_VERIFIED',fmt)
