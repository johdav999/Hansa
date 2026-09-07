import bpy,sys,pathlib,json,math
from mathutils import Vector
P=pathlib.Path(__file__).resolve().parents[1]
fmt=sys.argv[sys.argv.index('--')+1]
bpy.ops.wm.read_factory_settings(use_empty=True)
files=sorted((P/'exports').glob('SM_*.'+fmt));rows=[];objects=[]
for f in files:
 before=set(bpy.data.objects)
 if fmt=='glb':bpy.ops.import_scene.gltf(filepath=str(f))
 else:bpy.ops.import_scene.fbx(filepath=str(f))
 obs=[o for o in bpy.data.objects if o not in before and o.type=='MESH'];assert len(obs)==1
 ob=obs[0];ob.data.calc_loop_triangles();assert len(ob.data.loop_triangles)>0
 points=[ob.matrix_world@v.co for v in ob.data.vertices]
 rows.append({'file':f.name,'dimensions_m':[max(v[k] for v in points)-min(v[k] for v in points) for k in range(3)],'triangles':len(ob.data.loop_triangles),'materials':[m.name for m in ob.data.materials],'uv_layers':len(ob.data.uv_layers),'color_layers':len(ob.data.color_attributes)})
 objects.append(ob)
 if fmt=='fbx':
  # FBX cannot carry a vertex-color Multiply shader: reconstruct from delivered sidecar maps.
  m=ob.data.materials[0];m.use_nodes=True;n=m.node_tree.nodes;l=m.node_tree.links;bs=n.get('Principled BSDF')
  tex=n.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(P/'textures/dirt-road--basecolor--v1.png'))
  vc=n.new('ShaderNodeVertexColor');vc.layer_name=ob.data.vertex_colors[0].name
  mult=n.new('ShaderNodeMixRGB');mult.blend_type='MULTIPLY';mult.inputs[0].default_value=1;l.new(tex.outputs['Color'],mult.inputs[1]);l.new(vc.outputs['Color'],mult.inputs[2]);l.new(mult.outputs[0],bs.inputs['Base Color'])
  for fn,slot in [('Dirt_Normal','Normal'),('Dirt_Roughness','Roughness')]:
   im=bpy.data.images.load(str(P/'textures'/f'{fn}.png'));im.colorspace_settings.name='Non-Color';t=n.new('ShaderNodeTexImage');t.image=im
   if slot=='Normal':
    nm=n.new('ShaderNodeNormalMap');nm.inputs['Strength'].default_value=.5;l.new(t.outputs['Color'],nm.inputs['Color']);l.new(nm.outputs['Normal'],bs.inputs[slot])
   else:l.new(t.outputs[0],bs.inputs[slot])
  bs.inputs['Specular'].default_value=.22
 # Match source review offsets by mesh name.
 pos={'Straight':(-9,-10,0),'Corner':(7,-5,0),'End':(-9,0,0),'TJunction':(-9,12,0),'Crossroads':(8,12,0)}
 ob.location+=Vector(next(v for k,v in pos.items() if k in f.stem))
S=bpy.context.scene
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.15));g=bpy.context.object;gm=bpy.data.materials.new('Ground');gm.use_nodes=True;gm.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=(.10,.12,.10,1);gm.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value=.95;g.data.materials.append(gm)
w=bpy.data.worlds.new('Neutral');S.world=w;w.use_nodes=True;w.node_tree.nodes.get('Background').inputs[0].default_value=(.65,.74,.85,1);w.node_tree.nodes.get('Background').inputs[1].default_value=.7
d=bpy.data.lights.new('Sun','SUN');d.energy=2.5;d.angle=.15;o=bpy.data.objects.new('Sun',d);S.collection.objects.link(o);o.rotation_euler=(math.radians(32),math.radians(-25),math.radians(-32))
d=bpy.data.cameras.new('Kit');cam=bpy.data.objects.new('Kit',d);S.collection.objects.link(cam);cam.location=(34,-46,48);cam.rotation_euler=(Vector((0,2,0))-cam.location).to_track_quat('-Z','Y').to_euler();d.lens=45;S.camera=cam
S.render.engine='BLENDER_EEVEE';S.eevee.taa_render_samples=48;S.eevee.use_gtao=True;S.eevee.gtao_distance=.3
S.view_settings.view_transform='Filmic';S.view_settings.look='Medium High Contrast';S.render.resolution_x=1100;S.render.resolution_y=760;S.render.resolution_percentage=100;S.render.image_settings.file_format='JPEG';S.render.image_settings.quality=92
S.render.filepath=str(P/'renders'/f'reimport_{fmt}_Kit.jpg');bpy.ops.render.render(write_still=True)
cam.location=(-7,-14,3);cam.rotation_euler=(Vector((-9,-9,0))-cam.location).to_track_quat('-Z','Y').to_euler();d.lens=48;S.render.filepath=str(P/'renders'/f'reimport_{fmt}_Surface.jpg');bpy.ops.render.render(write_still=True)
assert len(rows)==5
(P/'evidence'/f'reimport_{fmt}.json').write_text(json.dumps(rows,indent=2))
print('REIMPORT_OK',fmt,rows)
