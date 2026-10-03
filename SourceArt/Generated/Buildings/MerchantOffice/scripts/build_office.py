import bpy
import math
import sys
from pathlib import Path
from mathutils import Vector

ROOT = next(p for p in Path(__file__).resolve().parents if (p / 'Hansa.uproject').exists())
JOB = ROOT / 'Saved/GenerationJobs/merchant-office-hausbaumhaus_20260930'
JOB.mkdir(parents=True,exist_ok=True)
TEX = ROOT / 'SourceArt/Generated/Buildings/MerchantOffice/texture-source'
VERSION = int(sys.argv[-1]) if sys.argv[-1].isdigit() else 4

bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system = 'METRIC'
bpy.context.scene.unit_settings.scale_length = 1.0

def image_mat(name, file, rough):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    n = mat.node_tree.nodes
    bsdf = n.get('Principled BSDF')
    bsdf.inputs['Roughness'].default_value = rough
    img = n.new('ShaderNodeTexImage')
    img.image = bpy.data.images.load(str(TEX / file), check_existing=True)
    img.image.pack()
    img.interpolation = 'Linear'
    mat.node_tree.links.new(img.outputs['Color'], bsdf.inputs['Base Color'])
    return mat

brick = image_mat('M_Office_DarkRedBrick', 'merchant-office--dark-brick--basecolor--1254x1254--v1.png', .83)
roof = image_mat('M_Office_ClayRoof', 'merchant-office--clay-roof--basecolor--1254x1254--v1.png', .78)
oak = image_mat('M_Office_DarkOak', 'merchant-office--dark-oak--basecolor--1254x1254--v1.png', .71)
def flat(name, color, rough=.75, metallic=0):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True
    b=m.node_tree.nodes.get('Principled BSDF');b.inputs['Base Color'].default_value=(*color,1)
    b.inputs['Roughness'].default_value=rough;b.inputs['Metallic'].default_value=metallic
    return m
stone=flat('M_Office_Sandstone',(.45,.35,.23),.88)
recess=flat('M_Office_ShadowedReveal',(.085,.052,.039),.9)
glass=flat('M_Office_SmokedGlass',(.067,.12,.14),.27)
iron=flat('M_Office_BlackIron',(.035,.031,.029),.65,.85)
plinth=flat('M_Office_DarkStone',(.22,.20,.18),.9)

def box(name, loc, size, mat, bevel=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o=bpy.context.object;o.name=name;o.dimensions=size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    o.data.materials.append(mat)
    o.data.use_auto_smooth=True
    if mat in {brick, roof, oak}:
        layer=o.data.uv_layers.active
        for poly in o.data.polygons:
            for li in poly.loop_indices:
                co=o.data.vertices[o.data.loops[li].vertex_index].co
                if abs(poly.normal.y)>.5: uv=(co.x/1.75,co.z/1.68)
                elif abs(poly.normal.x)>.5: uv=(co.y/1.75,co.z/1.68)
                else: uv=(co.x/1.75,co.y/1.68)
                layer.data[li].uv=uv
    if bevel:
        mod=o.modifiers.new('Soft masonry edges','BEVEL');mod.width=bevel;mod.segments=1
        o.modifiers.new('Weighted normals','WEIGHTED_NORMAL')
    return o

def plane_mesh(name, verts, faces, mat):
    me=bpy.data.meshes.new(name);me.from_pydata(verts,[],faces);me.update()
    o=bpy.data.objects.new(name,me);bpy.context.collection.objects.link(o);me.materials.append(mat)
    # planar UV per face; all roof planes use consistent repeatable map
    uv=me.uv_layers.new(name='UVMap')
    for p in me.polygons:
        for li in p.loop_indices:
            co=me.vertices[me.loops[li].vertex_index].co
            uv.data[li].uv=((co.y+3.35)/6.7,(co.x+3.6)/7.2)
    return o

# Street front is negative Y; the building origin is at ground and footprint centre.
box('Main brick hall 6.8 x 6.2 m',(0,0,3.73),(6.8,6.2,7.46),brick,.045)
box('Granite plinth',(0,0,.22),(6.98,6.38,.44),plinth,.035)
if VERSION>=2:
    # Parapet hides the roof edge and makes the defining stepped merchant gable.
    for i,(w,z) in enumerate([(6.92,8.00),(5.84,8.84),(4.78,9.68),(3.66,10.52),(2.52,11.36),(1.40,12.20)]):
        box('Brick stepped gable %02d'%i,(0,-3.21,z),(w,.35,.82),brick,.022)
        box('Gable coping %02d'%i,(0,-3.43,z+.42),(w+.10,.20,.09),stone,.012)
    for x in [-3.12,3.12]:
        box('Front corner pier %.1f'%x,(x,-3.30,4.01),(.28,.38,7.85),brick,.02)

def arch(name,cx,cz,rx,rz,y,material,width=.105):
    # Two near-circular haunches rise to a pointed Gothic crown.
    points=[]
    for i in range(11):
        t=math.pi - i*math.pi/2/10
        points.append((cx+rx*math.cos(t),y,cz+rz*math.sin(t)))
    for i in range(1,11):
        t=math.pi/2-i*math.pi/2/10
        points.append((cx+rx*math.cos(t),y,cz+rz*math.sin(t)))
    points[10]=(cx,y,cz+rz*1.16)
    curve=bpy.data.curves.new(name,'CURVE');curve.dimensions='3D';curve.bevel_depth=width/2;curve.bevel_resolution=1
    poly=curve.splines.new('POLY');poly.points.add(len(points)-1)
    for p,v in zip(poly.points,points):p.co=(*v,1)
    obj=bpy.data.objects.new(name,curve);bpy.context.collection.objects.link(obj);obj.data.materials.append(material)
    return obj

if VERSION>=2:
    # Three tall pointed street-level bays, based on the inspected Rostock facade.
    for i,x in enumerate([-2.18,0,2.18]):
        box('Ground arch deep reveal %d'%i,(x,-3.418,1.59),(1.66,.085,2.85),recess,.02)
        if i==1:
            box('Oak merchant door',(x,-3.475,1.50),(1.35,.10,2.55),oak,.025)
            box('Door crossbar',(x,-3.54,1.88),(1.35,.045,.07),iron,.008)
            box('Door iron ring',(x+.35,-3.565,1.28),(.07,.04,.10),iron,.015)
        else:
            box('Ground glazing %d'%i,(x,-3.480,1.70),(1.30,.045,1.65),glass,.01)
            box('Ground mullion %d'%i,(x,-3.52,1.70),(.065,.07,1.75),oak,.01)
            box('Ground sill %d'%i,(x,-3.53,.78),(1.55,.16,.13),stone,.012)
        arch('Pointed ground arch %d'%i,x,2.83,.82,.65,-3.525,stone,.105)
    # Upper hall windows remain within brick blind-arc recesses.
    for i,x in enumerate([-2.25,0,2.25]):
        box('Upper recessed bay %d'%i,(x,-3.395,5.34),(1.42,.085,2.56),recess,.012)
        box('Upper glazed opening %d'%i,(x,-3.465,5.15),(1.02,.048,1.72),glass,.012)
        box('Upper window oak mullion %d'%i,(x,-3.50,5.16),(.065,.065,1.78),oak,.008)
        box('Upper sill %d'%i,(x,-3.52,4.23),(1.35,.16,.12),stone,.01)
        arch('Upper pointed arcade %d'%i,x,6.44,.71,.53,-3.49,brick,.11)

if VERSION>=3:
    # Two tiled roof planes, ridge along the depth of the merchant hall.
    e=3.52;top=10.25;lo=7.35;front=-3.13;back=3.23
    plane_mesh('West clay tile roof',[(-e,front,lo),(0,front,top),(0,back,top),(-e,back,lo)],[(0,1,2,3)],roof)
    plane_mesh('East clay tile roof',[(0,front,top),(e,front,lo),(e,back,lo),(0,back,top)],[(0,1,2,3)],roof)
    box('Roof ridge timber',(0,.05,10.27),(.14,6.55,.12),iron,.02)
    for x in [-3.48,3.48]:box('Eaves drip edge %.2f'%x,(x,.05,7.32),(.12,6.65,.13),iron,.02)
    for level,(x,z) in enumerate([(0,8.08),(-.96,9.01),(.96,9.01),(0,10.0),(0,11.07)]):
        box('Gable lancet shadow %d'%level,(x,-3.425,z),(.48,.05,.94),recess,.012)
        arch('Gable lancet brick cap %d'%level,x,z+.46,.24,.17,-3.47,stone,.045)
    # Rear stock door and narrow side service openings.
    box('Rear oak stock door',(0,3.18,1.45),(1.65,.13,2.65),oak,.025)
    for x in [-2.25,2.25]:
        box('Rear upper light %.2f'%x,(x,3.17,5.2),(.77,.08,1.25),glass,.015)
        box('Rear light sill %.2f'%x,(x,3.25,4.5),(.92,.16,.12),stone,.01)
    for side in [-1,1]:
        for y in [-1.45,1.5]:
            box('Side shutter %d %.2f'%(side,y),(side*3.43,y,4.85),(.09,.73,1.23),oak,.016)
    # Goods winch and exposed iron tie ends are visible only after the massing pass.
    box('Winch beam',(0,-3.67,11.40),(.17,.75,.19),oak,.014)
    bpy.ops.mesh.primitive_torus_add(major_radius=.13,minor_radius=.025,location=(0,-4.02,11.18),rotation=(math.pi/2,0,0))
    bpy.context.object.name='Winch iron hook';bpy.context.object.data.materials.append(iron)

if VERSION>=4:
    # Final correction: deepen shadowed front portals and strengthen the gable rhythm.
    for x in [-2.18,0,2.18]:
        box('Portal edge brick %.2f'%x,(x-0.89,-3.59,1.58),(.14,.18,2.9),brick,.009)
        box('Portal opposite edge brick %.2f'%x,(x+0.89,-3.59,1.58),(.14,.18,2.9),brick,.009)
    for x in [-2.72,-1.60,-.55,.55,1.60,2.72]:
        box('Gable vertical pilaster %.2f'%x,(x,-3.48,8.02),(.12,.10,.76),brick,.007)

if VERSION>=5:
    # The first engine orbit revealed the open rear roof triangle. Close it with
    # actual brick geometry so both the gameplay and review cameras are sound.
    verts=[(-3.36,3.12,7.46),(3.36,3.12,7.46),(0,3.12,10.18),
           (-3.36,3.28,7.46),(3.36,3.28,7.46),(0,3.28,10.18)]
    faces=[(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)]
    me=bpy.data.meshes.new('RearGableBrick');me.from_pydata(verts,[],faces);me.update()
    o=bpy.data.objects.new('Rear triangular brick infill',me);bpy.context.collection.objects.link(o);me.materials.append(brick)
    uv=me.uv_layers.new(name='UVMap')
    for face in me.polygons:
        for li in face.loop_indices:
            co=me.vertices[me.loops[li].vertex_index].co
            uv.data[li].uv=(co.x/1.75,co.z/1.68)

# Native review cameras and neutral daylight.
world=bpy.context.scene.world;world.color=(.65,.68,.72)
def target(obj,pt):obj.rotation_euler=(Vector(pt)-obj.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(13,-18,13));cam=bpy.context.object;cam.name='Camera_StreetThreeQuarter';target(cam,(0,0,5.7));cam.data.type='ORTHO';cam.data.ortho_scale=19
bpy.context.scene.camera=cam
bpy.ops.object.light_add(type='AREA',location=(-8,-10,17));sun=bpy.context.object;sun.data.energy=3600;sun.data.shape='DISK';sun.data.size=9;target(sun,(0,0,5))
if VERSION>=4:
    bpy.ops.object.light_add(type='AREA',location=(9,3,12));fill=bpy.context.object;fill.name='Neutral side fill';fill.data.energy=2200;fill.data.shape='DISK';fill.data.size=8;target(fill,(0,0,5))
bpy.context.scene.render.engine='BLENDER_EEVEE';bpy.context.scene.eevee.use_gtao=True
bpy.context.scene.render.resolution_x=800;bpy.context.scene.render.resolution_y=800;bpy.context.scene.render.resolution_percentage=100
bpy.context.scene.view_settings.view_transform='Standard';bpy.context.scene.view_settings.look='Medium High Contrast'
bpy.context.scene.camera.data.lens=45
bpy.context.scene.render.image_settings.file_format='PNG'
render=JOB / ('iteration-%d.png'%VERSION)
bpy.context.scene.render.filepath=str(render)
bpy.ops.render.render(write_still=True)
if VERSION==5:
    master=JOB / 'SM_MerchantOffice_Hausbaumhaus.blend'
    bpy.ops.wm.save_as_mainfile(filepath=str(master))
    # Export only geometry, not lighting or camera.
    bpy.ops.object.select_all(action='DESELECT')
    for o in bpy.context.scene.objects:
        if o.type in {'MESH','CURVE'}:
            o.select_set(True)
            if o.type=='CURVE':bpy.context.view_layer.objects.active=o
    bpy.ops.object.convert(target='MESH')
    bpy.ops.export_scene.gltf(filepath=str(JOB/'SM_MerchantOffice_Hausbaumhaus.glb'),export_format='GLB',use_selection=True,export_apply=True)
    bpy.ops.export_scene.fbx(filepath=str(JOB/'SM_MerchantOffice_Hausbaumhaus.fbx'),use_selection=True,apply_unit_scale=True,axis_forward='-Y',axis_up='Z',path_mode='COPY',embed_textures=True)
print('HANSA_OFFICE_DONE',VERSION,render)
