"""Clean-room FBX/GLB reload and original-size render checks; no source overwrites."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector
REPO=Path(__file__).resolve().parents[1]
JOB=REPO/'Saved/GenerationJobs/hansa-vehicles_P19_20260908'
manifest=json.loads((JOB/'exports/export-manifest.json').read_text())
results={}
for fmt in ('fbx','glb'):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.render.engine='CYCLES';scene.cycles.samples=24
    scene.render.resolution_x=1280;scene.render.resolution_y=720;scene.render.resolution_percentage=100
    scene.world=bpy.data.worlds.new('Neutral');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.65,.69,.72,1)
    scene.view_settings.view_transform='Filmic';scene.view_settings.look='Medium High Contrast'
    results[fmt]={};roles={}
    for name,entry in manifest['modules'].items():
        before=set(bpy.data.objects)
        path=str(JOB/'exports'/entry[fmt])
        if fmt=='fbx':bpy.ops.import_scene.fbx(filepath=path)
        else:bpy.ops.import_scene.gltf(filepath=path)
        objs=[o for o in bpy.data.objects if o not in before and o.type=='MESH'];assert objs,name
        points=[o.matrix_world@Vector(c) for o in objs for c in o.bound_box]
        bounds=[[min(p[i] for p in points),max(p[i] for p in points)] for i in range(3)]
        expected=[[min(p[i] for p in entry['boundsMetres']),max(p[i] for p in entry['boundsMetres'])] for i in range(3)]
        assert max(abs(bounds[i][j]-expected[i][j]) for i in range(3) for j in range(2))<.002,(name,bounds,expected)
        assert all(o.data.uv_layers for o in objs),name
        for o in objs:o.location+=Vector(manifest['roleOffsetsMetres'][name])
        roles[name]=objs;results[fmt][name]={'boundsMetres':bounds,'uvPresent':True}
    for o in roles['SM_HansaCog_FurledSail']:o.hide_render=True
    for x,y in ((-.92,-10.09),(.92,-10.09),(.92,-11.91)):
        for src in roles['SM_HansaWagon_Wheel']:
            ob=src.copy();scene.collection.objects.link(ob);ob.location+=Vector((x+.92,y+11.91,0))
    bpy.ops.object.light_add(type='AREA',location=(10,-15,28));light=bpy.context.object;light.data.energy=5500;light.data.size=12
    light.rotation_euler=(Vector((0,0,4))-light.location).to_track_quat('-Z','Y').to_euler()
    for tag,pos,target,lens in (('whole',(38,-44,29),(0,0,6.5),48),('wagon',(6,-17,4),(1,-11,.8),52)):
        bpy.ops.object.camera_add(location=pos);cam=bpy.context.object;cam.data.lens=lens
        cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();scene.camera=cam
        scene.render.filepath=str(JOB/'renders'/f'reimport-{fmt}-{tag}.png');bpy.ops.render.render(write_still=True)
    bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(JOB/'checkpoints'/f'reimport-{fmt}.blend'))
(JOB/'evidence/reimport.json').write_text(json.dumps(results,indent=2))
print('P19_REIMPORT_PASS')
