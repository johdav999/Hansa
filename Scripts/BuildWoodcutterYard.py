"""Headless editable woodcutter yard; independent original reconstruction in metres."""
import bpy, math, random, json, sys
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
JOB=ROOT/'SourceArt/Generated/Buildings/HansaWoodcutterYard_20260916'
REV=int(sys.argv[sys.argv.index('--')+1]) if '--' in sys.argv else 1
OUT=JOB/f'review-r{REV:02d}'; OUT.mkdir(parents=True,exist_ok=True)
random.seed(316)
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene; scene.unit_settings.system='METRIC'; scene.unit_settings.scale_length=1
scene.render.engine='CYCLES'; scene.cycles.samples=32
scene.render.resolution_x=1200; scene.render.resolution_y=900; scene.render.resolution_percentage=100
scene.view_settings.view_transform='Filmic'; scene.view_settings.look='Medium High Contrast'
scene.world.color=(.3,.3,.3)
def material(name,color,rough=.8,texture=None):
    m=bpy.data.materials.new(name);m.use_nodes=True
    p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*color,1);p.inputs['Roughness'].default_value=rough
    if texture:
        tex=m.node_tree.nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(texture),check_existing=True)
        m.node_tree.links.new(tex.outputs['Color'],p.inputs['Base Color'])
        if REV>=3:
            tint=m.node_tree.nodes.new('ShaderNodeMixRGB');tint.blend_type='MULTIPLY';tint.inputs[0].default_value=1
            tint.inputs[2].default_value=(*((.56,.49,.40) if name=='ShelteredOak_ImageGen' else (.8,.73,.63)),1)
            m.node_tree.links.new(tex.outputs['Color'],tint.inputs[1]);m.node_tree.links.new(tint.outputs[0],p.inputs['Base Color'])
        # Independent procedural relief; color stains are never used as height.
        noise=m.node_tree.nodes.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=110;noise.inputs['Detail'].default_value=2
        bump=m.node_tree.nodes.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.12;bump.inputs['Distance'].default_value=.001
        uv=m.node_tree.nodes.new('ShaderNodeTexCoord');m.node_tree.links.new(uv.outputs['UV'],noise.inputs['Vector'])
        m.node_tree.links.new(noise.outputs['Fac'],bump.inputs['Height']);m.node_tree.links.new(bump.outputs['Normal'],p.inputs['Normal'])
    return m
oak=material('Oak_ImageGen',(.3,.21,.12),texture=JOB/'texture-source/oak--1254x1254--v1.png')
dark=material('ShelteredOak_ImageGen',(.24,.17,.1),texture=JOB/'texture-source/oak--1254x1254--v1.png')
fresh=material('SplitEndgrain_Procedural',(.57,.40,.22))
bark=material('Bark',(.18,.13,.085),texture=JOB/'texture-source/oak--1254x1254--v1.png')
iron=material('ForgedIron',(.045,.04,.033),.62);iron.node_tree.nodes.get('Principled BSDF').inputs['Metallic'].default_value=.8
stone=material('FoundationStone',(.28,.27,.24),.94)
objects=[]
def box(name,loc,scale,mat,bevel=.012):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name=name;o.dimensions=scale
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    o.data.materials.append(mat)
    if bevel:
        b=o.modifiers.new('HandWornEdges','BEVEL');b.width=bevel;b.segments=2
        o.modifiers.new('WeightedCornerNormals','WEIGHTED_NORMAL')
    objects.append(o);return o
def beam(name,a,b,width,mat):
    a,b=Vector(a),Vector(b);o=box(name,(a+b)/2,(width,width,(b-a).length),mat)
    o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler();return o
def log(name,loc,length,radius=.16,split=False):
    # Axis X; real irregular polygon ends, bark along sides and separate pale endgrain.
    n=7 if not split else 3;verts=[]
    for x in [-length/2,length/2]:
        for i in range(n):
            a=math.tau*i/n;rr=radius*random.uniform(.91,1.08);verts.append((x,math.cos(a)*rr,math.sin(a)*rr))
    faces=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(o);o.location=loc
    mesh.materials.append(bark);mesh.materials.append(fresh)
    for p in mesh.polygons:p.material_index=1 if p.index<2 else 0
    objects.append(o);return o
# 8 x 6 m shelter within a 12 x 12 m building plot; road front is local +X.
for x in [-3,0,3]:
    for y in [-2.35,2.35]:
        box('StonePad',(x,y,.13),(.48,.48,.26),stone,.045)
        box('OakPost',(x,y,1.63),(.24,.24,3),oak)
        if REV>=2:
            for sign in ([1] if x==-3 else [-1] if x==3 else [-1,1]):beam('KneeBrace',(x,y,2.35),(x+sign*.65,y,3.0),.14,oak)
for y in [-2.35,2.35]:beam('WallPlate',(-3.3,y,3.05),(3.3,y,3.05),.24,oak)
for x in [-3,0,3]:
    beam('TieBeam',(x,-2.5,3),(x,2.5,3),.2,oak)
    beam('Rafter',(x,-2.75,2.95),(x,0,4.7),.17,oak)
    beam('Rafter',(x,0,4.7),(x,2.75,2.95),.17,oak)
beam('Ridge',(-3.6,0,4.7),(3.6,0,4.7),.18,oak)
# Overlapping wooden shingles are an explicit reconstruction choice, not the museum's clay roof.
pitch=math.atan2(1.75,2.75)
for side in [-1,1]:
    for row in range(12):
        y=side*(2.75-row*.24);z=(3.10 if REV>=2 else 2.98)+row*.24*math.tan(pitch)+(.006*row if REV>=2 else 0)
        for col in range(20):
            x=-3.65+col*.375+(row%2)*.12
            o=box('RoofShingle',(x,y,z),(.39,.46,.036 if REV<3 else .025),dark,.007)
            o.rotation_euler[0]=-side*pitch
            if REV>=3:o.rotation_euler[2]=random.uniform(-.012,.012)
# Rear slat wall and two drying racks with airflow, open working front.
for i in range(23):box('BackSlat',(-3.10,-2.2+i*.2,1.6),(.08,.17,2.7),oak,.008)
for side in [-1,1]:
    for y in [side*1.25,side*2.15]:
        beam('RackRail',(-2.5,y,.34),(2.4,y,.34),.12,oak)
    for row in range(6):
        for col in range(14-row//2):
            o=log('SplitFirewood',(-2.15+col*.31,side*1.68,.57+row*.23),.9,.17,True)
            o.rotation_euler[2]=math.pi/2
            if REV>=2:o.rotation_euler[0]=random.uniform(-.65,.65);o.location.z+=random.uniform(-.035,.035)
for i in range(5):log('InputTimber',(3.6,-1.8+i*.38,.3),2.0,.2)
stump=log('ChoppingBlock',(4.1,1.1,.38),.7,.38);stump.rotation_euler[1]=math.pi/2
beam('AxeHandle',(4.12,1.1,.65),(4.4,1.1,1.4),.055,oak)
box('AxeHead',(4.41,1.1,1.34),(.30,.075,.16),iron,.015)
box('Workbench',(1.9,.1,.94),(1.4,.6,.12),oak)
for x in [1.3,2.5]:
    for y in [-.15,.35]:box('BenchLeg',(x,y,.48),(.10,.10,.90),oak)
if REV>=3:
    for i in range(9):
        o=log('LooseSplit',(3.7+random.uniform(-.7,.7),2.3+random.uniform(-.4,.4),.15),.5,.105,True);o.rotation_euler[2]=random.uniform(0,3)
# Face projection in metres, with image V along each member's longitudinal axis.
for o in objects:
    mesh=o.data
    uv=mesh.uv_layers.new(name='UVMap') if not mesh.uv_layers else mesh.uv_layers.active
    dims=o.dimensions
    grain=1 if o.name.startswith('RoofShingle') else 0 if any(o.name.startswith(n) for n in ['SplitFirewood','InputTimber','LooseSplit','ChoppingBlock']) else max(range(3),key=lambda a:dims[a])
    for poly in mesh.polygons:
        normal_axis=max(range(3),key=lambda a:abs(poly.normal[a]))
        axes=[a for a in range(3) if a!=normal_axis]
        v=grain if grain in axes else axes[1];u=next(a for a in axes if a!=v)
        for idx in poly.loop_indices:
            co=mesh.vertices[mesh.loops[idx].vertex_index].co
            uv.data[idx].uv=(co[u],co[v])

ground=box('PreviewGround',(0,0,-.08),(20,20,.1),material('PreviewLinen',(.3,.28,.22)),0)
objects.remove(ground)
def camera(name,loc,target,scale):
    bpy.ops.object.camera_add(location=loc);c=bpy.context.object;c.name=name;c.rotation_euler=(Vector(target)-c.location).to_track_quat('-Z','Y').to_euler();c.data.type='ORTHO';c.data.ortho_scale=scale;return c
hero=camera('Review_Hero',(13,-15,12),(0,0,1.7),15)
close=camera('Review_Timber',(7,-7,5),(1,0,1.5),7)
bpy.ops.object.light_add(type='AREA',location=(2,-7,11));bpy.context.object.data.energy=2200;bpy.context.object.data.size=8
bpy.ops.object.light_add(type='AREA',location=(-6,4,7));bpy.context.object.data.energy=1400;bpy.context.object.data.size=7
for cam,name in [(hero,'hero'),(close,'timber-close')]:
    scene.camera=cam;scene.render.filepath=str(OUT/f'{name}.png');bpy.ops.render.render(write_still=True)
scene.camera=hero
for im in bpy.data.images:
    if im.source=='FILE':im.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'woodcutter-yard.blend'))
stats={'revision':REV,'objects':len(objects),'vertices':sum(len(o.data.vertices) for o in objects),'source_texture':'oak--1254x1254--v1.png','scale':'metres','plot':[12,12],'status':'draft, requires inspection'}
(OUT/'model.json').write_text(json.dumps(stats,indent=2))
print(json.dumps(stats))
