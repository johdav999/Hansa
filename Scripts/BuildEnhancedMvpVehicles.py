"""Editable P19 Bremen-type cargo cog and bounded local wagon. Source metres."""
import sys, shutil
from pathlib import Path
REPO=Path(__file__).resolve().parents[1]
JOB=REPO/'Saved/GenerationJobs/hansa-vehicles_P19_20260908'
for directory in ('textures','checkpoints','renders','exports','evidence'):(JOB/directory).mkdir(parents=True,exist_ok=True)
SOURCE=REPO/'SourceArt/Generated/Buildings/HansaMarket_P15_20260908'
for name in ('oak-source.png','clay-source.png','canvas-source.png'):
    if not (JOB/'textures'/name).exists():shutil.copy2(SOURCE/'textures'/name,JOB/'textures'/name)
helper=(SOURCE/'scripts/build_market.py').read_text().split("core=group('SM_HansaMarket')")[0]
exec(helper.replace('JOB=Path(__file__).resolve().parents[1];','').replace('M_Market_','M_Vehicle_'))
old_tube=tube
def tube(name,points,r,mat,col,sides=6,closed=False):
    ob=old_tube(name,points,r,mat,col,sides,closed)
    if REV>=3:
        lengths=[0]
        for i in range(1,len(points)):lengths.append(lengths[-1]+(Vector(points[i])-Vector(points[i-1])).length)
        for p in ob.data.polygons:
            for li in p.loop_indices:
                vi=ob.data.loops[li].vertex_index
                ob.data.uv_layers.active.data[li].uv=(vi%sides/sides*math.tau*r/2,lengths[vi//sides]/2)
    return ob
s.cycles.samples=24
linen=material('Linen',(.7,.65,.53),.9,'linen-source.png',.00025)
tar=oak.copy();tar.name='M_Vehicle_TarredOak'
next(n for n in tar.node_tree.nodes if n.type=='HUE_SAT').inputs['Value'].default_value=.22
rope=material('Hemp',(.22,.16,.085),.87,relief=.0002)

cog=group('SM_HansaCog_Hull')
# Broad working hull, archaeological 23.27 x 7.62 m envelope. The rig is inferred.
L=11.635
def sheer(x):return 2.80+1.30*(abs(x)/L)**4
def width(x):return 3.81*max(0,1-(x/L)**2)**.55
def hull(x,t):
    z=-2+ t*(sheer(x)+2)
    w=width(x)*(.24+.76*math.sin(t*math.pi/2))
    return w,z
N=64;rows=12 if REV>=2 else 8
for side in (-1,1):
    for row in range(rows):
        verts=[];faces=[];uv=[]
        for i in range(N+1):
            x=-L+2*L*i/N
            for edge in (0,1):
                t=(row+edge)/rows-(.008 if REV>=3 and edge==0 and row else 0);w,z=hull(x,t)
                # True overlapping strake lip; not a painted joint grid.
                w+=.035 if edge==0 and REV>=2 else 0
                verts.append((x,side*w,z));uv.append((t*(sheer(x)+2)/2,x/2))
        for i in range(N):faces.append((2*i,2*i+2,2*i+3,2*i+1))
        if side>0:faces=[tuple(reversed(f)) for f in faces]
        o=mesh('Clinker strake',verts,faces,tar,cog,uv)
        sol=o.modifiers.new('Oak plank thickness','SOLIDIFY');sol.thickness=.055
        for p in o.data.polygons:p.use_smooth=True
    tube('Sheer wale',[(-L+2*L*i/N,side*width(-L+2*L*i/N),sheer(-L+2*L*i/N)) for i in range(N+1)],.095,oak,cog,8)
# Flush bottom and deck boards, clipped to the hull; real hold opening.
for i in range(70):
    x=-11.3+i*.326;w=width(x)
    box('Carvel bottom plank',(x,0,-1.97),(.318,max(.12,w*.48),.12),tar,cog,.006)
    deckw=max(.1,min(w*.96,hull(x,(2.22+2)/(sheer(x)+2))[0]-.09)) if REV>=4 else max(.1,w*.96)
    if -3.4<x<-.3:
        for side in (-1,1):box('Deck beside hatch',(x,side*(1.05+deckw)/2,2.22),(.318,max(.05,deckw-1.05),.12),oak,cog,.006)
    else:box('Deck board',(x,0,2.22),(.318,deckw*2,.12),oak,cog,.006)
for y in (-1.10,1.10):box('Cargo hatch coaming',(-1.85,y,2.4),(3.35,.14,.4),oak,cog)
for x in (-3.5,-.2):box('Cargo hatch end',(x,0,2.4),(.14,2.3,.4),oak,cog)
box('Dark hold floor',(-1.85,0,1.85 if REV>=3 else 1.2),(3.3,2.3,.15),tar,cog)
for x in (-11.6,11.6):beam('Stem post',(x,0,-1.8),(x,0,4.22),.20,oak,cog)
box('Sternpost rudder',(-11.82,0,-.25),(.45,.16,3.8),tar,cog)
beam('Tiller',(-11.7,0,2.1),(-8.5,0,2.9),.12,oak,cog)
if REV>=4:
    for y in (-.46,.46):box('Mast partner',(.6,y,2.38),(1.05,.22,.30),oak,cog)
    for y in (-.8,.8):box('Windlass cheek',(5.4,y,2.72),(.16,.20,.96),oak,cog)
    tube('Working windlass',[(5.4,-1.0,2.96),(5.4,1.0,2.96)],.20,oak,cog,12)
    for y in (-1.10,1.10):beam('Windlass lever',(5.4,y,2.48),(5.4,y,3.44),.075,oak,cog)
if REV>=2:
    for x in (-9.8,-8.6,-7.4):
        for side in (-1,1):beam('Stern working platform support',(x,side*2.25,2.3),(x,side*2.25,4.45),.14,oak,cog)
    for i in range(15):box('Stern platform board',(-9.9+i*.22,0,4.45),(.21,4.65,.10),oak,cog,.008)
    for side in (-1,1):
        for x in (-9.9,-8.5,-6.85):beam('Stern rail stanchion',(x,side*2.26,4.4),(x,side*2.26,5.4),.10,oak,cog)
        beam('Stern rail',(-10,side*2.26,5.4),(-6.75,side*2.26,5.4),.11,oak,cog)
    beam('Stern transverse rail',(-10,-2.26,5.4),(-10,2.26,5.4),.11,oak,cog)
    for i in range(7):box('Stern ladder tread',(-6.5-i*.16,0,2.45+i*.29),(.26,.70,.07),oak,cog,.005)
    if REV>=3:
        for y in (-.30,.30):beam('Ladder stringer',(-6.34,y,2.2),(-7.59,y,4.45),.095,oak,cog)
rig=group('SM_HansaCog_Rig')
tube('Main mast',[(.6,0,2.0),(.6,0,17.7)],.24,oak,rig,16)
tube('Square yard',[(.6,-6.15,15.65),(.6,6.15,15.65)],.13,oak,rig,12)
for side in (-1,1):
    for x in (-3,0,3):tube('Standing shroud',[(x,side*2.9,2.9),(.6,side*.12,16.8)],.026,rope,rig,6)
for x in (-10.8,10.9):tube('Fore and aft stay',[(x,0,3.9),(.6,0,17)],.035,rope,rig,6)
for side in (-1,1):tube('Sheet',[(2.0,side*5.8,7.0),(-5.6,side*2.8,3.2)],.028,rope,rig,6)
sail=group('SM_HansaCog_Sail')
verts=[];uv=[];faces=[]
for iz in range(19):
    t=iz/18
    for iy in range(25):
        u=iy/24; y=(u-.5)*(11.6-.4*t)
        x=.62+(1.65 if REV>=2 else .5)*math.sin(math.pi*u)*math.sin(math.pi*t)
        z=7.0+8.55*t+.28*math.sin(math.pi*u)*(1-t)
        verts.append((x,y,z));uv.append((y/.5,z/.5))
for iz in range(18):
    for iy in range(24):
        a=iz*25+iy;faces.append((a,a+1,a+26,a+25))
o=mesh('Plain square linen sail',verts,faces,linen,sail,uv)
sol=o.modifiers.new('Sail thickness','SOLIDIFY');sol.thickness=.006
for p in o.data.polygons:p.use_smooth=True
furled=group('SM_HansaCog_FurledSail')
tube('Furled linen bundle',[(.7,-5.7,15.5),(.7,5.7,15.5)],.23,linen,furled,12)
for o in furled.objects:o.hide_render=True
if REV>=3:
    for y in (-5.7,-3.8,-1.9,0,1.9,3.8,5.7):
        tube('Yard sail tie',[(.6,y,15.9),(.83,y,15.6)],.018,rope,rig,6)
    for side in (-1,1):
        for x in (-5,4,7):
            beam('Mooring timber',(x,side*(width(x)-.18),2.3),(x,side*(width(x)-.18),3.4),.17,oak,cog)
    # Lapped cloths are geometric seam cords, not stretched painted lines.
    for iy in range(2,24,2):
        tube('Linen panel seam',[verts[iz*25+iy] for iz in range(19)],.008,linen,sail,4)
cargo=group('SM_HansaCargo_Sacks')
for x in (-.38,.38):
    for y in (-.32,.32):
        ellipsoid('Tied grain sack',(x,y,.34),(.34,.27,.34),canvas,cargo)
        tube('Sack neck',[(x,y,.55),(x,y,.70)],.065,canvas,cargo,8)
        tube('Sack tie',[(x+.075*math.cos(i*math.tau/16),y+.075*math.sin(i*math.tau/16),.60) for i in range(16)],.012,rope,cargo,5,True)
wagon=group('SM_HansaWagon_Body')
for y in (-.48,.48):box('Wagon longitudinal chassis',(0,y,.62),(2.8,.17,.20),oak,wagon)
for i in range(13):box('Cargo bed plank',(-1.26+i*.21,0,.83),(.20,1.45,.11),oak,wagon,.006)
for y in (-.74,.74):
    for z in (1.02,1.23,1.44):box('Removable side plank',(0,y,z),(2.8,.065,.19),oak,wagon,.006)
    for x in (-1.3,0,1.3):box('Stake',(x,y,1.16),(.095,.105,.87),oak,wagon)
for x in (-1.38,1.38):
    for z in (1.02,1.23,1.44):box('Tail board',(x,0,z),(.075,1.5,.19),oak,wagon,.006)
for x in (-.92,.92):beam('Wooden axle',(x,-1.03,.56),(x,1.03,.56),.13,oak,wagon)
for y in (-.50,.50):beam('Draft shaft',(1.0,y,.69),(3.5,y*.7,.77),.095,oak,wagon)
beam('Draft crossbar',(2.3,-.50,.72),(2.3,.50,.72),.07,oak,wagon)
wheel=group('SM_HansaWagon_Wheel')
# Wheel local origin at axle center, rotation axis Y. 1.12 m diameter.
for r,mat,thick in ((.50,oak,.065),(.556,iron,.018)):
    if REV<3:
        tube('Felloe' if mat==oak else 'Iron tyre',[(r*math.cos(i*math.tau/48),0,r*math.sin(i*math.tau/48)) for i in range(48)],thick,mat,wheel,8,True)
    else:
        vv=[];ff=[];uv=[]
        for i in range(49):
            a=i*math.tau/48
            for radius,y in ((r-thick,-.065),(r+thick,-.065),(r+thick,.065),(r-thick,.065)):
                vv.append((radius*math.cos(a),y,radius*math.sin(a)));uv.append(((radius if REV>=4 else y)/2,i/48*math.tau*r/2))
        for i in range(48):
            for j in range(4):ff.append((i*4+j,i*4+(j+1)%4,(i+1)*4+(j+1)%4,(i+1)*4+j))
        ob=mesh('Continuous felloe' if mat==oak else 'Forged tyre',vv,ff,mat,wheel,uv)
        if REV>=4:
            ob.data.use_auto_smooth=True
            for p in ob.data.polygons:p.use_smooth=True
            b=ob.modifiers.new('Worked rim edge','BEVEL');b.width=.006;b.segments=2
tube('Hub',[(0,-.14,0),(0,.14,0)],.12,oak,wheel,12)
for i in range(10 if REV>=2 else 8):
    a=i*math.tau/(10 if REV>=2 else 8)
    beam('Radial spoke',(.10*math.cos(a),0,.10*math.sin(a)),(.50*math.cos(a),0,.50*math.sin(a)),.055,oak,wheel)
offsets={cog.name:(0,0,0),rig.name:(0,0,0),sail.name:(0,0,0),furled.name:(0,0,0),cargo.name:(-1.9,0,1.93 if REV>=3 else 2.3),wagon.name:(0,-11,0),wheel.name:(-.92,-11.91,.575)}
for name,col in groups.items():
    for ob in col.objects:ob.location+=Vector(offsets[name]);ob['previewOffset']=offsets[name]
preview=group('ReviewWheelInstances')
for x,y in ((-.92,-10.09),(.92,-10.09),(.92,-11.91)):
    for src in wheel.objects:
        ob=src.copy();preview.objects.link(ob);ob.location+=Vector((x+.92,y+11.91,0))
ground=group('ReviewGround');mat=material('ReviewGround',(.21,.26,.26),.9)
box('Ground',(0,0,-2.25),(100,100,.05),mat,ground,0)
if REV>=3:box('Wagon review ground',(0,-11,-.05),(8,5,.1),mat,ground,0)
bpy.ops.object.light_add(type='AREA',location=(10,-15,28));light=bpy.context.object;light.data.energy=5500;light.data.size=12
light.rotation_euler=(Vector((0,0,4))-light.location).to_track_quat('-Z','Y').to_euler()
def camera(name,pos,target,lens):
    bpy.ops.object.camera_add(location=pos);o=bpy.context.object;o.name=name;o.data.lens=lens;o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler();return o
whole=camera('CogWhole',(38,-44,29),(0,0,6.5),48)
detail=camera('CogDeck',(9,-15,14),(-1,0,2.7),48)
cart=camera('WagonDetail',(6,-17,4),(1,-11,.8),52)
import bmesh
for col in groups.values():
    for ob in col.objects:
        if ob.type=='MESH':
            bm=bmesh.new();bm.from_mesh(ob.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(ob.data);bm.free()
bpy.ops.wm.save_as_mainfile(filepath=str(JOB/'checkpoints'/f'Vehicles-r{REV}.blend'))
for cam,tag in ((whole,'cog'),(detail,'deck'),(cart,'wagon')):
    s.camera=cam;s.render.filepath=str(JOB/'renders'/f'r{REV}-{tag}.png');bpy.ops.render.render(write_still=True)
(JOB/'evidence'/f'layout-r{REV}.json').write_text(json.dumps({'revision':REV,'offsets':offsets,'waterline':0,'deck':2.25,'status':'draft-unverified'},indent=2))
