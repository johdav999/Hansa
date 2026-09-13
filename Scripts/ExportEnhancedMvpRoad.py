"""P18 packed master, native shader bakes and individual portable exports."""
import bpy,json,shutil
from pathlib import Path
REPO=Path(__file__).resolve().parents[1];JOB=REPO/'Saved/GenerationJobs/hansa-road_P18_20260908'
bpy.ops.wm.open_mainfile(filepath=str(JOB/'checkpoints/Road-r4.blend'))
s=bpy.context.scene;source=bpy.data.materials['M_Road_Earth'];nodes=source.node_tree.nodes;links=source.node_tree.links
bs=nodes.get('Principled BSDF');out=nodes.get('Material Output')
# A swatch has no shoulder vertex colors: bake the opaque surface, not transparent black.
alpha_source=bs.inputs['Alpha'].links[0].from_socket
links.remove(bs.inputs['Alpha'].links[0]);bs.inputs['Alpha'].default_value=1
bpy.ops.mesh.primitive_plane_add(size=2,location=(0,0,0));plane=bpy.context.object;plane.data.materials.append(source)
for o in s.objects:o.select_set(False)
plane.select_set(True);bpy.context.view_layer.objects.active=plane
s.cycles.samples=4;s.render.bake.margin=0
images={}
for channel,kind in [('BaseColor','EMIT'),('Roughness','ROUGHNESS'),('Normal','NORMAL')]:
    im=bpy.data.images.new('M_Road_Earth_'+channel,width=1024,height=1024,alpha=False)
    im.colorspace_settings.name='sRGB' if channel=='BaseColor' else 'Non-Color'
    target=nodes.new('ShaderNodeTexImage');target.image=im;nodes.active=target
    if channel=='BaseColor':
        emission=nodes.new('ShaderNodeEmission');color=bs.inputs['Base Color'].links[0].from_socket
        links.new(color,emission.inputs['Color']);links.new(emission.outputs[0],out.inputs['Surface'])
    bpy.ops.object.bake(type=kind)
    im.filepath_raw=str(JOB/'exports'/('M_Road_Earth_'+channel+'.png'));im.file_format='PNG';im.save();images[channel]=im
    if channel=='BaseColor':links.new(bs.outputs[0],out.inputs['Surface']);nodes.remove(emission)
    nodes.remove(target)
bpy.data.objects.remove(plane,do_unlink=True)
links.new(alpha_source,bs.inputs['Alpha'])
portable=bpy.data.materials.new('M_Road_Earth_Portable');portable.use_nodes=True
n=portable.node_tree.nodes;l=portable.node_tree.links;p=n.get('Principled BSDF')
for channel,im in images.items():
    t=n.new('ShaderNodeTexImage');t.image=im
    if channel=='Normal':
        nm=n.new('ShaderNodeNormalMap');l.new(t.outputs['Color'],nm.inputs['Color']);l.new(nm.outputs[0],p.inputs['Normal'])
    else:l.new(t.outputs['Color'],p.inputs['Base Color' if channel=='BaseColor' else 'Roughness'])
edge=n.new('ShaderNodeVertexColor');edge.layer_name='RoadBlend';l.new(edge.outputs['Alpha'],p.inputs['Alpha']);portable.blend_method='HASHED'
records={}
for ob in [o for o in s.objects if o.name.startswith('SM_HansaRoad_')]:
    loc=ob.location.copy();ob.location=(0,0,0);ob.data.materials[0]=portable
    for o in s.objects:o.select_set(False)
    ob.select_set(True);bpy.context.view_layer.objects.active=ob
    # A thin authored slab is simple collision data, never complex-as-simple runtime navigation.
    bpy.ops.mesh.primitive_cube_add(size=1,location=(0,0,-.025));ucx=bpy.context.object;ucx.name='UCX_'+ob.name+'_00';ucx.dimensions=(4,2.8,.05)
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    ob.select_set(True)
    bpy.ops.export_scene.fbx(filepath=str(JOB/'exports'/(ob.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,global_scale=1,add_leaf_bones=False,use_mesh_modifiers=True,mesh_smooth_type='FACE',path_mode='COPY')
    bpy.data.objects.remove(ucx,do_unlink=True)
    ob.select_set(True);bpy.context.view_layer.objects.active=ob
    bpy.ops.export_scene.gltf(filepath=str(JOB/'exports'/(ob.name+'.glb')),export_format='GLB',use_selection=True,export_texcoords=True,export_normals=True,export_colors=True)
    records[ob.name]={'fbx':ob.name+'.fbx','glb':ob.name+'.glb','boundsMin':[min(v.co[k] for v in ob.data.vertices) for k in range(3)],'boundsMax':[max(v.co[k] for v in ob.data.vertices) for k in range(3)]}
    ob.location=loc;ob.data.materials[0]=source
for im in bpy.data.images:
    if im.source=='FILE':im.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(JOB/'exports/HansaRoad.blend'))
(JOB/'exports/export-manifest.json').write_text(json.dumps({'revision':4,'nativeCellMetres':4,'nativeBakeDimensions':[1024,1024],'sourceInputDimensions':[1254,1254],'physicalTextureMetres':2,'normalConvention':'OpenGL +Y','modules':records},indent=2))
print('P18_EXPORTED',len(records))
