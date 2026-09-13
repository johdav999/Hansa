"""P20 editable, manifest-bounded street furniture. Blender metres; no world writes."""
import sys, shutil
from pathlib import Path
REPO = Path(__file__).resolve().parents[1]
JOB = REPO / 'Saved/GenerationJobs/city-life_P20_20260908'
SOURCE = REPO / 'SourceArt/Generated/Buildings/HansaMarket_P15_20260908'
for folder in ('textures', 'checkpoints', 'renders', 'exports', 'evidence'):
    (JOB/folder).mkdir(parents=True, exist_ok=True)
for name in ('oak-source', 'clay-source', 'canvas-source'):
    for extension in ('.png', '.prompt.md'):
        shutil.copy2(SOURCE/'textures'/(name+extension), JOB/'textures'/(name+extension))
helper = (SOURCE/'scripts/build_market.py').read_text().split("core=group('SM_HansaMarket')")[0]
exec(helper.replace('JOB=Path(__file__).resolve().parents[1];', '').replace('M_Market_', 'M_CityLife_'))
random.seed(2020)

def tube(name, points, r, mat, col, sides=6, closed=False):
    """Use one-sided endpoint tangents; never wrap an open tube's last ring."""
    assert len(points) >= 2 and sides >= 3 and r > 0
    points = [Vector(p) for p in points]
    vertices, faces = [], []
    for i, point in enumerate(points):
        before = points[(i-1) % len(points)] if closed else points[max(0, i-1)]
        after = points[(i+1) % len(points)] if closed else points[min(len(points)-1, i+1)]
        tangent = after-before
        assert tangent.length > .00001, (name, i, 'degenerate tangent')
        tangent.normalize()
        rotation = tangent.to_track_quat('Z', 'Y')
        for j in range(sides):
            offset = rotation @ Vector((r*math.cos(j*math.tau/sides), r*math.sin(j*math.tau/sides), 0))
            assert abs(offset.dot(tangent)) < .000001, (name, i, 'non-planar ring')
            vertices.append(point+offset)
    for i in range(len(points) if closed else len(points)-1):
        for j in range(sides):
            faces.append((i*sides+j, i*sides+(j+1)%sides,
                          ((i+1)%len(points))*sides+(j+1)%sides, ((i+1)%len(points))*sides+j))
    if not closed:
        faces.extend([tuple(reversed(range(sides))), tuple((len(points)-1)*sides+j for j in range(sides))])
    obj = mesh(name, vertices, faces, mat, col)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj

# Plain civic oak chest-bench, based on the inspected fifteenth-century Met
# 41.190.277 structure; exterior finish and omission of carving are interpretations.
bench = group('SM_HansaBench_Oak')
for x in (-.85,.85):
    for y in (-.21,.21): box('Bench corner post',(x,y,.40),(.10,.11,.8),oak,bench)
    box('Arm cap',(x,0,.82),(.17,.58,.08),oak,bench)
box('Bench seat',(0,0,.47),(1.78,.49,.065),oak,bench)
for y in (-.22,.22):
    for z in (.12,.42): box('Mortised rail',(0,y,z),(1.7,.065,.09),oak,bench)
    for x in (-.60,-.30,0,.30,.60): box('Joined panel',(x,y,.27),(.285,.035,.23),oak,bench,.006)
    if REV >= 5:
        for x in (-.775,-.45,-.15,.15,.45,.775):box('Panel stile',(x,y,.27),(.075,.055,.25),oak,bench,.004)
for x in (-.85,.85): box('Bench end',(x,0,.27),(.05,.43,.30),oak,bench)
if REV >= 2:
    box('Chest floor',(0,0,.105),(1.7,.42,.04),oak,bench)
    for x in (-.6,.6):box('Seat hinge',(x,.245,.44),(.045,.018,.15),iron,bench,.003)
    box('Seat hasp',(0,-.25,.425),(.04,.018,.14),iron,bench,.003)

# Lübeck archaeology supports oak-lined wells; this above-ground winding gear
# is explicitly an inferred functional reconstruction, not a surveyed monument.
well = group('SM_HansaWell_Oak')
for i in range(6):
    z=.08+i*.155
    for y in (-.65,.65): box('Well lining',(0,y,z),(1.42,.12,.15),oak,well)
    for x in (-.65,.65): box('Well lining',(x,0,z),(.12,1.2,.15),oak,well)
for x in (-.67,.67):
    box('Well upright',(x,0,1.15),(.16,.18,2.3),oak,well)
    beam('Windlass brace',(x,-.48,.75),(x,0,1.5),.10,oak,well)
tube('Windlass',[(-.84,0,1.88),(.92,0,1.88)],.105,oak,well,12)
beam('Winding crank',(.95,0,1.88),(.95,0,1.56),.055,iron,well)
tube('Handle',[(.95,0,1.56),(1.12,0,1.56)],.04,oak,well,8)
tube('Well rope',[(0,0,.18),(0,0,1.9)],.012,wicker,well,6)
if REV >= 3:
    tube('Wound windlass rope',[( -.21+.42*j/120,.12*math.sin(j*math.tau/20),1.88+.12*math.cos(j*math.tau/20)) for j in range(121)],.012,wicker,well,6)
box('Dark well recess',(0,0,.015),(1.15,1.15,.025),oak,well,0)
if REV >= 2:
    for x in (-.67,.67):box('Well coping',(x,0,.97),(.19,1.5,.10),oak,well)
    for y in (-.67,.67):box('Well coping',(0,y,.97),(1.17,.19,.10),oak,well)
    for i in range(16):
        a=i*math.tau/16
        beam('Bucket stave',(.15*math.cos(a),.15*math.sin(a),.25),(.18*math.cos(a),.18*math.sin(a),.55),.057,oak,well)
    for z,r in ((.29,.155),(.51,.18)):
        tube('Bucket hoop',[(r*math.cos(j*math.tau/24),r*math.sin(j*math.tau/24),z) for j in range(24)],.012,iron,well,5,True)
    tube('Bucket handle',[(-.18,0,.54),(-.12,0,.68),(.12,0,.68),(.18,0,.54)],.012,iron,well,5)

sign = group('SM_HansaSign_Balance')
box('Signpost',(-.36,0,1.35),(.12,.12,2.7),oak,sign)
box('Sign arm',(.06,0,2.59),(.96,.10,.10),oak,sign)
beam('Sign brace',(-.36,0,2.0),(.40,0,2.55),.065,oak,sign)
box('Blank sign board',(.20,0,2.12),(.62,.045,.55),oak,sign)
for x in (-.04,.44):tube('Sign suspension',[(x,0,2.36),(x,0,2.57)],.009,iron,sign,5)
# Physical balance pictogram: no baked or changing text and no material decal.
for y in (-.036,.036):
    beam('Balance stem',(.20,y,1.94),(.20,y,2.28),.023,iron,sign)
    beam('Balance arm',(-.02,y,2.21),(.42,y,2.21),.023,iron,sign)
    for x in (0,.4):
        beam('Scale chain',(x,y,2.21),(x,y,2.01),.012,iron,sign)
        tube('Scale pan',[(x-.07,y,2.04),(x,y,1.99),(x+.07,y,2.04)],.018,iron,sign,5)

for gate in (False,True):
    col=group('SM_HansaFence_Gate' if gate else 'SM_HansaFence_Rail2m')
    for x in (-1,1):box('Fence post',(x,0,.58),(.14,.14,1.16),oak,col)
    for z in (.40,.88):box('Fence rail',(0,0,z),(2,.075,.09),oak,col)
    if gate:
        for x in (-.80,-.40,0,.40,.80):box('Gate pale',(x,0,.61),(.065,.08,.92),oak,col)
        beam('Gate diagonal',(-.84,-.06,.2),(.84,-.06,1.05),.065,oak,col)
        if REV >= 2:
            for z in (.4,.88):box('Gate hinge strap',(-.79,-.055,z),(.32,.014,.04),iron,col,.002)
            box('Gate latch',(.88,-.055,.88),(.25,.014,.04),iron,col,.002)

barrel=group('SM_HansaBarrel_Coopered')
N=20
for i in range(N):
    a=i*math.tau/N;verts=[]
    for z,r in ((0,.28),(.10,.30),(.44,.35),(.78,.30),(.88,.28)):
        for edge in (-.48,.48):
            ang=a+edge*math.tau/N;verts.append((r*math.cos(ang),r*math.sin(ang),z))
    o=mesh('Separate stave',verts,[(2*j,2*j+1,2*j+3,2*j+2) for j in range(4)],oak,barrel,
           [(edge*.10,z/2) for z in (0,.10,.44,.78,.88) for edge in (0,1)])
    o.modifiers.new('Stave thickness','SOLIDIFY').thickness=.025
for z,r in ((.10,.301),(.22,.3186),(.65,.3191),(.78,.301)):
    if REV < 3:
        tube('Split wooden hoop',[(r*math.cos(j*math.tau/40),r*math.sin(j*math.tau/40),z) for j in range(40)],.016,wicker,barrel,6,True)
    else:
        slope=(.05/.34 if z<.44 else -.05/.34) if REV>=5 else 0
        verts=[((r+dr+slope*dz)*math.cos(j*math.tau/40),(r+dr+slope*dz)*math.sin(j*math.tau/40),z+dz) for j in range(40) for dr,dz in ((0,-.025),(.012,-.025),(.012,.025),(0,.025))]
        mesh('Flat split-wood binding',verts,[(4*j+k,4*((j+1)%40)+k,4*((j+1)%40)+(k+1)%4,4*j+(k+1)%4) for j in range(40) for k in range(4)],oak,barrel)
for z in (.035,.85):
    for k in range(7):
        x=(k-3)*.074;half=math.sqrt(max(0,.27**2-x*x))
        box('Coopered head',(x,0,z),(.071,half*2,.025),oak,barrel,.003)

debris=group('SM_HansaShoreDebris_Driftwood')
for i in range(3):
    a=i*.6;length=1.2-i*.25
    branch=tube('Water-worn branch',[(-length/2,i*.14,.065),(0,i*.14+.06,.09),(length/2,i*.14-.06,.05)],.04,oak,debris,8)
    fork=tube('Fork',[(0,i*.14,.09),(.28,i*.14+.32,.055)],.021,oak,debris,6)
    if REV >= 4:
        for ob,sides,factors in ((branch,8,(.45,1,.2)),(fork,6,(1,.15))):
            for row,factor in enumerate(factors):
                ring=list(ob.data.vertices)[row*sides:(row+1)*sides]
                center=sum((v.co for v in ring),Vector())/sides
                for v in ring:v.co=center+(v.co-center)*factor

# Ground pivot is part of the export contract, not an actor-placement workaround.
debris_min_z = min((o.matrix_world @ v.co).z for o in debris.objects for v in o.data.vertices)
for o in debris.objects:
    o.location.z -= debris_min_z

offsets={}
for i,(name,col) in enumerate(groups.items()):
    delta=Vector(((i%4)*3.6-5.4,(i//4)*4.0-2,0));offsets[name]=list(delta)
    for o in col.objects:o.location+=delta;o['previewOffset']=list(delta)
ground=group('ReviewGround');gm=material('ReviewGround',(.22,.25,.24),.95)
box('Review surface',(0,0,-.06),(200,200,.1),gm,ground,0)
bpy.ops.object.light_add(type='AREA',location=(5,-8,13));light=bpy.context.object;light.data.energy=2600;light.data.size=8
light.rotation_euler=(Vector((0,0,1))-light.location).to_track_quat('-Z','Y').to_euler()
def camera(name,pos,target,lens):
    bpy.ops.object.camera_add(location=pos);o=bpy.context.object;o.name=name;o.data.lens=lens
    o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler();return o
cameras=[camera('FamilyCamera',(13,-18,15),(0,0,1),48),camera('JoineryCamera',(-3,-7,3),(-4,-2,.7),48)]
if REV >= 4:
    cameras += [camera('BenchCamera',(-3.5,-5,1.6),(-5.4,-2,.4),42),camera('CooperCamera',(1.4,-1,2),(-1.8,2,.45),52)]
(JOB/'evidence'/'source-layout.json').write_text(json.dumps({'revision':REV,'offsets':offsets},indent=2))
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(JOB/'checkpoints'/('CityProps-r'+str(REV)+'.blend')))
for c,tag in zip(cameras,('whole','detail','bench','barrel')):
    s.camera=c;s.render.filepath=str(JOB/'renders'/('r'+str(REV)+'-'+tag+'.png'));bpy.ops.render.render(write_still=True)
print('P20_DRAFT_RENDER_COMPLETE',REV)
