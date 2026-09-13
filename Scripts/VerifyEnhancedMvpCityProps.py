"""Verify each P20 delivery module in clean Blender contexts, not the source scene."""
import bpy,json,sys
from pathlib import Path
from mathutils import Vector
REPO=Path(__file__).resolve().parents[1]
JOB=REPO/'Saved/GenerationJobs/city-life_P20_20260908'
REV = int(sys.argv[-1]) if sys.argv[-1].isdigit() else 5
EXPORT_DIR = 'exports' if REV == 5 else 'exports-r'+str(REV)
SUFFIX = '' if REV == 5 else '-r'+str(REV)
manifest=json.loads((JOB/EXPORT_DIR/'export-manifest.json').read_text())
assert manifest['revision'] == REV
results={}
for fmt in ('fbx','glb'):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.samples=16
    s.render.resolution_x=1280;s.render.resolution_y=720;s.render.resolution_percentage=100
    s.world=bpy.data.worlds.new('Neutral daylight');s.world.use_nodes=True
    s.world.node_tree.nodes['Background'].inputs[0].default_value=(.67,.73,.8,1)
    s.world.node_tree.nodes['Background'].inputs[1].default_value=.65
    s.view_settings.view_transform='Filmic';s.view_settings.look='Medium High Contrast'
    results[fmt]={}
    for name,entry in manifest['modules'].items():
        before=set(bpy.data.objects)
        path=str(JOB/EXPORT_DIR/entry[fmt])
        if fmt=='fbx':bpy.ops.import_scene.fbx(filepath=path)
        else:bpy.ops.import_scene.gltf(filepath=path)
        objs=[o for o in bpy.data.objects if o not in before and o.type=='MESH'];assert objs
        points=[o.matrix_world@Vector(v) for o in objs for v in o.bound_box]
        bounds=[[min(p[a] for p in points),max(p[a] for p in points)] for a in range(3)]
        expected=[[min(p[a] for p in entry['boundsMetres']),max(p[a] for p in entry['boundsMetres'])] for a in range(3)]
        assert max(abs(bounds[a][b]-expected[a][b]) for a in range(3) for b in range(2))<.002,(name,bounds,expected)
        assert all(o.data.uv_layers and len(o.data.materials)>0 for o in objs)
        if 'ShoreDebris' in name:
            assert abs(bounds[2][0]) < .0001, (name, 'ground pivot', bounds)
        results[fmt][name]={'boundsMetres':bounds,'uvPresent':True,'materialCount':sum(len(o.data.materials) for o in objs)}
        for o in objs:o.location+=Vector(manifest['roleOffsetsMetres'][name])
    bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.01));floor=bpy.context.object
    mat=bpy.data.materials.new('ReviewOnly');mat.diffuse_color=(.22,.25,.24,1);floor.data.materials.append(mat)
    bpy.ops.object.light_add(type='AREA',location=(5,-8,13));light=bpy.context.object;light.data.energy=2600;light.data.size=8
    light.rotation_euler=(Vector((0,0,1))-light.location).to_track_quat('-Z','Y').to_euler()
    for tag,pos,target,lens in (('whole',(13,-18,15),(0,0,1),48),('bench',(-3.5,-5,1.6),(-5.4,-2,.4),42),('barrel',(1.4,-1,2),(-1.8,2,.45),52)):
        bpy.ops.object.camera_add(location=pos);c=bpy.context.object;c.data.lens=lens
        c.rotation_euler=(Vector(target)-c.location).to_track_quat('-Z','Y').to_euler();s.camera=c
        s.render.filepath=str(JOB/'renders'/f'reimport-{fmt}-{tag}{SUFFIX}.png');bpy.ops.render.render(write_still=True)
    bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(JOB/'checkpoints'/f'reimport-{fmt}{SUFFIX}.blend'))
(JOB/('evidence/reimport'+SUFFIX+'.json')).write_text(json.dumps(results,indent=2))
print('P20_PARTIAL_REIMPORT_PASS')
