"""P18 native 4m overlay geometry; preserved ImageGen input and independent PBR channels."""
import bpy, math, json, sys, shutil
from pathlib import Path
from mathutils import Vector
REPO=Path(__file__).resolve().parents[1]
JOB=REPO/'Saved/GenerationJobs/hansa-road_P18_20260908'
OLD=REPO/'SourceArt/Generated/Roads/HansaDirtRoad_20260906_01'
REV=int(sys.argv[-1])
for d in ('textures','renders','exports','evidence','checkpoints'): (JOB/d).mkdir(parents=True,exist_ok=True)
for name in ('dirt-road--basecolor--v1.png','dirt-road--basecolor--v1.prompt.md','Dirt_Normal.png','Dirt_Roughness.png'):
    shutil.copy2(OLD/'textures'/name,JOB/'textures'/name)
bpy.ops.wm.read_factory_settings(use_empty=True)
s=bpy.context.scene;s.unit_settings.system='METRIC';s.unit_settings.scale_length=1
s.render.engine='CYCLES';s.cycles.samples=24;s.cycles.use_denoising=True
s.render.resolution_x=1280;s.render.resolution_y=720;s.render.resolution_percentage=100
s.view_settings.view_transform='Standard';s.view_settings.look='Medium High Contrast';s.view_settings.exposure=0
m=bpy.data.materials.new('M_Road_Earth');m.use_nodes=True
nodes=m.node_tree.nodes;links=m.node_tree.links;bs=nodes.get('Principled BSDF')
base=bpy.data.images.load(str(JOB/'textures/dirt-road--basecolor--v1.png'));base.colorspace_settings.name='sRGB'
tex=nodes.new('ShaderNodeTexImage');tex.image=base
links.new(tex.outputs['Color'],bs.inputs['Base Color'])
bs.inputs['Specular'].default_value=.22
if REV>=3:
    edge=nodes.new('ShaderNodeVertexColor');edge.layer_name='RoadBlend'
    links.new(edge.outputs['Alpha'],bs.inputs['Alpha']);m.blend_method='HASHED'
for name,slot in [('Dirt_Normal','Normal'),('Dirt_Roughness','Roughness')]:
    image=bpy.data.images.load(str(JOB/'textures'/f'{name}.png'));image.colorspace_settings.name='Non-Color'
    n=nodes.new('ShaderNodeTexImage');n.image=image
    if slot=='Normal':
        nm=nodes.new('ShaderNodeNormalMap');nm.inputs['Strength'].default_value=.12
        links.new(n.outputs['Color'],nm.inputs['Color']);links.new(nm.outputs['Normal'],bs.inputs['Normal'])
    else: links.new(n.outputs['Color'],bs.inputs['Roughness'])

ports={'Isolated':[],'End':[(2,0)],'Straight':[(2,0),(-2,0)],'Corner':[(2,0),(0,-2)],'TJunction':[(2,0),(0,-2),(-2,0)],'Crossroads':[(2,0),(0,-2),(-2,0),(0,2)]}
def clamp(x): return max(0,min(1,x))
def smooth(x): x=clamp(x);return x*x*(3-2*x)
def distance(x,y,ends):
    if not ends:return math.hypot(x,y)
    return min(math.hypot(x-a*clamp((x*a+y*b)/4),y-b*clamp((x*a+y*b)/4)) for a,b in ends)
objects=[];manifest={}
for idx,(role,ends) in enumerate(ports.items()):
    radius=1.2 if not ends else 1.4
    def sdf(p):return distance(*p,ends)-radius
    vertices=[];faces=[];lookup={}
    def add(p):
        x,y=p;key=(round(x,7),round(y,7))
        if key in lookup:return lookup[key]
        d=distance(x,y,ends);shoulder=smooth((d-(.95 if REV<2 else 1.05))/(radius-(.95 if REV<2 else 1.05)))
        crown=(.055 if REV<2 else .025)*max(0,1-(d/radius)**2)
        z=crown-(.055 if REV<2 else .025)*shoulder
        # No end-noise: connector profile must match at every rotation.
        if REV>=3:
            z+=.0015*math.sin(x*5)*math.sin(y*5)*math.sin(math.pi*x/2)**2*math.sin(math.pi*y/2)**2
            if ends:z-=.009*math.exp(-((d-.62)/.15)**2)*smooth((max(abs(x),abs(y))-.4)/.6)
        lookup[key]=len(vertices);vertices.append((x,y,z));return len(vertices)-1
    def clip(poly):
        out=[]
        for p,q in zip(poly,poly[1:]+poly[:1]):
            a,b=sdf(p),sdf(q)
            if a<=0:out.append(p)
            if (a<0)!=(b<0):
                t=a/(a-b);out.append((p[0]+t*(q[0]-p[0]),p[1]+t*(q[1]-p[1])))
        return out
    n=32
    for i in range(n):
        for j in range(n):
            x=-2+i*4/n;y=-2+j*4/n;h=4/n
            poly=clip([(x,y),(x+h,y),(x+h,y+h),(x,y+h)])
            if len(poly)>=3:
                ids=[add(p) for p in poly]
                for k in range(1,len(ids)-1):
                    tri=(ids[0],ids[k],ids[k+1])
                    if len(set(tri))==3:faces.append(tri)
    mesh=bpy.data.meshes.new(role);mesh.from_pydata(vertices,[],faces);mesh.update()
    assert not mesh.validate(verbose=True),role
    ob=bpy.data.objects.new('SM_HansaRoad_'+role,mesh);s.collection.objects.link(ob);mesh.materials.append(m)
    mesh.uv_layers.new(name='UV0_2m');mesh.vertex_colors.new(name='RoadBlend')
    uv=mesh.uv_layers[0];vc=mesh.vertex_colors[0]
    for face in mesh.polygons:
        face.use_smooth=True
        for li in face.loop_indices:
            v=mesh.vertices[mesh.loops[li].vertex_index].co;uv.data[li].uv=(v.x/2,v.y/2)
            alpha=1-smooth((distance(v.x,v.y,ends)-(radius-.55))/.4)
            vc.data[li].color=(1,1,1,alpha)
    objects.append(ob)
    manifest[ob.name]={'role':role,'sourcePorts':ends,'triangles':len(mesh.polygons),'vertices':len(mesh.vertices),'boundsMin':[min(v[k] for v in vertices) for k in range(3)],'boundsMax':[max(v[k] for v in vertices) for k in range(3)],'nativeCellMetres':4,'origin':[0,0,0]}
    ob.location=((idx%3-1)*5,(idx//3-.5)*5,0)

s.world=bpy.data.worlds.new('Neutral');s.world.use_nodes=True;s.world.node_tree.nodes['Background'].inputs[0].default_value=(.65,.7,.76,1);s.world.node_tree.nodes['Background'].inputs[1].default_value=.65
bpy.ops.object.light_add(type='AREA',location=(0,-8,14));light=bpy.context.object;light.data.energy=1700;light.data.size=8
light.rotation_euler=(Vector((0,0,0))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.mesh.primitive_plane_add(size=60,location=(0,0,0 if REV>=2 else -.028));ground=bpy.context.object;ground.name='ReviewGround'
gm=bpy.data.materials.new('ReviewSoil');gm.diffuse_color=(.23,.25,.19,1);ground.data.materials.append(gm)
def camera(name,pos,target,lens):
    bpy.ops.object.camera_add(location=pos);cam=bpy.context.object;cam.name=name;cam.data.lens=lens
    cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();return cam
whole=camera('RoadKit',(13,-18,20),(0,0,0),48)
close=camera('RoadDetail',(4,-8,5),(0,-2.5,0),52)
for image in bpy.data.images:
    if image.source=='FILE':image.pack()
if REV>=4:
    # World-space sampling eliminates texture phase/rotation discontinuities at joins.
    geo=nodes.new('ShaderNodeNewGeometry');scale=nodes.new('ShaderNodeVectorMath');scale.operation='SCALE';scale.inputs[3].default_value=.5
    links.new(geo.outputs['Position'],scale.inputs[0])
    for n in list(nodes):
        if n.type=='TEX_IMAGE':links.new(scale.outputs[0],n.inputs['Vector'])
    # A connected sample exercises T, cross, corner, end, straight and isolated forms.
    cells={(x,0) for x in range(-3,4)}|{(0,y) for y in range(-2,3)}|{(3,1),(3,2),(2,2),(1,2),(5,3)}
    choices=[(0,0),(1,0),(1,1),(3,0),(1,2),(2,0),(3,1),(4,0),(1,3),(3,3),(2,1),(4,3),(3,2),(4,2),(4,1),(5,0)]
    for x,y in sorted(cells):
        mask=sum(1<<i for i,(dx,dy) in enumerate([(1,0),(0,-1),(-1,0),(0,1)]) if (x+dx,y+dy) in cells)
        tile,turn=choices[mask];copy=objects[tile].copy();copy.data=objects[tile].data.copy();s.collection.objects.link(copy)
        copy.name='Connected_'+str(x)+'_'+str(y);copy.location=(x*4,14+y*4,0);copy.rotation_euler.z=-turn*math.pi/2
    connected=camera('RoadConnected',(22,-17,28),(2,14,0),42)
bpy.ops.wm.save_as_mainfile(filepath=str(JOB/'checkpoints'/f'Road-r{REV}.blend'))
for cam,tag in [(whole,'kit'),(close,'detail')]:
    s.camera=cam;s.render.filepath=str(JOB/'renders'/f'r{REV}-{tag}.png');bpy.ops.render.render(write_still=True)
if REV>=4:
    s.camera=connected;s.render.filepath=str(JOB/'renders'/f'r{REV}-connected.png');bpy.ops.render.render(write_still=True)
(JOB/'evidence'/f'geometry-r{REV}.json').write_text(json.dumps(manifest,indent=2))
print('P18_SOURCE_RENDERED',REV)
