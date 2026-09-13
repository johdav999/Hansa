"""Original P17 modular timber harbor; metric source, deck datum Z=0, +X water.
Reuses immutable P15 ImageGen color inputs and geometry helpers, not a purchased model.
"""
import sys
from pathlib import Path
REPO = Path(__file__).resolve().parents[1]
JOB = REPO / 'Saved/GenerationJobs/hansa-harbor_P17_20260908'
for directory in ('textures','checkpoints','renders','exports','evidence'):
    (JOB / directory).mkdir(parents=True, exist_ok=True)
import shutil
SOURCE = REPO / 'SourceArt/Generated/Buildings/HansaMarket_P15_20260908'
for name in ('oak-source.png','clay-source.png','canvas-source.png'):
    shutil.copy2(SOURCE/'textures'/name, JOB/'textures'/name)
helper = (SOURCE/'scripts/build_market.py').read_text().split("core=group('SM_HansaMarket')")[0]
helper = helper.replace("JOB=Path(__file__).resolve().parents[1];", "")
exec(helper.replace('M_Market_', 'M_Harbor_'))
wet = oak.copy(); wet.name = 'M_Harbor_WetOak'
bs = wet.node_tree.nodes.get('Principled BSDF')
hue = next(n for n in wet.node_tree.nodes if n.type == 'HUE_SAT')
hue.inputs['Value'].default_value = .29
rough = next(n for n in wet.node_tree.nodes if n.type == 'MAP_RANGE')
rough.inputs['To Min'].default_value = .42; rough.inputs['To Max'].default_value = .57

deck = group('SM_HansaDock_Deck4m')
for i in range(16):
    box('Hewn deck plank',(0,-1.875+i*.25,-.07),(4,.239,.14),oak,deck,.008)
for y in (-1.5,1.5):
    box('Longitudinal bearer',(0,y,-.25),(4,.22,.36),oak,deck)
    for x in (-1.6,1.6):
        tube('Pile above splash',[(x,y,-2.05),(x,y,-.25)],.15,oak,deck,10)
        tube('Submerged pile',[(x,y,-4),(x,y,-2.05)],.15,wet,deck,10)
        if REV >= 2:
            beam('Braced pile frame',(x,y,-1.7),(-x,y,-.35),.12,oak,deck)
if REV >= 3:
    for x in (-1.6,1.6):
        for y in (-1.5,1.5):
            tube('Timber treenail',[(x,y,-.015),(x,y,.006)],.021,oak,deck,8)

edge = group('SM_HansaQuay_Edge4m')
for i in range(10):
    z=-.12-i*.22
    box('Retaining timber course',(0,0,z),(.25,4,.207),wet if z < -1.8 else oak,edge)
for y in (-1.7,1.7):
    box('Retaining pile',(.18,y,-1.75),(.22,.23,3.5),wet,edge)
box('Quay capping',(-.05,0,-.06),(.65,4,.12),oak,edge)

corner = group('SM_HansaQuay_Corner')
for i in range(10):
    z=-.12-i*.22
    box('Corner return X',(.24,0,z),(.48,.24,.207),wet if z < -1.8 else oak,corner)
    box('Corner return Y',(0,.24,z),(.24,.48,.207),wet if z < -1.8 else oak,corner)
box('Corner cap',(.13,.13,-.06),(.52,.52,.12),oak,corner)

steps = group('SM_HansaPier_Steps')
for i in range(6):
    box('Landing stair tread',(0,i*.28,-.1-i*.18),(1.2,.29,.14),oak,steps)
for x in (-.49,.49):
    beam('Stair stringer',(x,-.1,-.22),(x,1.55,-1.2),.14,oak,steps)
if REV >= 2:
    for y,z in ((0,0),(1.4,-.9)):
        beam('Stair handrail upright',(-.53,y,z),(-.53,y,z+.9),.075,oak,steps)
    beam('Stair handrail',(-.53,0,.9),(-.53,1.4,0),.07,oak,steps)

moor = group('SM_HansaMooring_Post')
tube('Mooring post',[(0,0,0),(0,0,.8)],.13,oak,moor,12)
beam('Mooring cross pin',(0,-.24,.58),(0,.24,.58),.07,oak,moor)
if REV >= 3:
    for z in (.34,.37,.40):
        tube('Hemp loop',[(.15*math.cos(i*math.tau/32),.15*math.sin(i*math.tau/32),z) for i in range(32)],.014,wicker,moor,6,True)

hoist = group('SM_HansaHarborHoist')
# Original compact manual windlass interpretation, not the monumental Gdansk crane.
for y in (-.5,.5):
    box('Hoist sill',(-.35,y,.10),(1.8,.20,.20),oak,hoist)
    beam('Hoist upright',(0,y,.2),(0,y,3.3),.20,oak,hoist)
    beam('Hoist back brace',(-1.15,y,.2),(0,y,2.6),.13,oak,hoist)
beam('Lifting head',(0,0,3.4),(1.8,0,3.4),.23,oak,hoist)
if REV>=5: beam('Crosshead support',(0,-.62,3.28),(0,.62,3.28),.20,oak,hoist)
beam('Lifting head brace',(0,0,2.2),(1.45,0,3.3),.14,oak,hoist)
tube('Manual windlass drum',[(0,-.65,1.0),(0,.65,1.0)],.16,oak,hoist,16)
beam('Manual windlass handle',(0,.79,.55),(0,.79,1.45),.07,oak,hoist)
for y in (-.07,.07):
    tube('Pulley cheeks',[((1.90 if REV>=5 else 1.68)+.16*math.cos(i*math.tau/24),y,(3.40 if REV>=5 else 3.22)+.16*math.sin(i*math.tau/24)) for i in range(24)],.028,iron,hoist,6,True)
if REV>=5:
    tube('Rear rope sheave',[(-.03+.15*math.cos(i*math.tau/24),0,3.40+.15*math.sin(i*math.tau/24)) for i in range(24)],.027,iron,hoist,8,True)
    for y in (-.1,.1):
        beam('Pulley mounting cheek',(1.55,y,3.40),(1.92,y,3.40),.07,iron,hoist)
rope = [(0,0,1.15),(0,0,3.40),(1.67,0,3.40),(1.82,0,3.24),(1.82,0,.7)]
if REV >= 4: rope = [(0,0,1.15),(-.18,0,1.4),(-.18,0,3.55),(1.67,0,3.55),(1.87,0,3.3),(1.87,0,.7)]
if REV>=5: rope=[(0,0,1.15),(-.18,0,1.4),(-.18,0,3.40),(-.13,0,3.50),(-.03,0,3.55),(1.90,0,3.56),(2.01,0,3.51),(2.06,0,3.40),(2.06,0,.7)]
tube('Continuous unloaded hoist rope',rope,.015,wicker,hoist,6)
hook_x=2.06 if REV>=5 else 1.87 if REV>=4 else 1.82
tube('Empty lifting hook',[(hook_x,0,.7),(hook_x,0,.45),(hook_x+.11,0,.38),(hook_x+.18,0,.50)],.02,iron,hoist,8)

cargo = group('SM_HansaHarborTransferSkid')
for y in (-.45,.45): box('Hewn runner',(0,y,.09),(1.4,.14,.18),oak,cargo)
for x in (-.5,-.25,0,.25,.5): box('Loose-board skid',(x,0,.20),(.23,1.1,.06),oak,cargo)
# Empty transfer equipment deliberately avoids inventing authoritative cargo.

offsets={deck.name:(0,0,0),edge.name:(-8,2,0),corner.name:(-8,4,0),steps.name:(-6,1.9 if REV>=2 else 3,0),moor.name:(7,1.75,0),hoist.name:(5.8,1,0),cargo.name:(-6,-1.2,0)}
for name,col in list(groups.items()):
    for o in col.objects:
        o.location += Vector(offsets[name]); o['previewOffset']=offsets[name]
preview=group('PreviewInstances')
for offset in (-6,-2,2,6):
    for src in deck.objects:
        o=src.copy();preview.objects.link(o);o.location.x+=offset
for src in deck.objects: src.hide_render=True
for src in edge.objects:
    o=src.copy();preview.objects.link(o);o.location.y-=4
ground=group('ReviewGround')
water=material('ReviewWater',(.04,.12,.15),.2)
box('Review water datum',(0,0,-2.28),(80,80,.06),water,ground,0)
bpy.ops.object.light_add(type='AREA',location=(10,-12,20));light=bpy.context.object
light.data.energy=3500;light.data.size=8;light.rotation_euler=(Vector((0,0,0))-light.location).to_track_quat('-Z','Y').to_euler()
def camera(name,position,target,lens):
    bpy.ops.object.camera_add(location=position);o=bpy.context.object;o.name=name;o.data.lens=lens
    o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler();return o
whole=camera('HarborWhole',(21,-24,18),(0,0,-.1),43)
detail=camera('HarborDetail',(11,-9,6),(6.3,.5,1.6),52)
import bmesh
for col in groups.values():
    for o in col.objects:
        if o.type=='MESH':
            bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free()
bpy.ops.wm.save_as_mainfile(filepath=str(JOB/'checkpoints'/('Harbor-r'+str(REV)+'.blend')))
for cam,tag in ((whole,'whole'),(detail,'detail')):
    s.camera=cam;s.render.filepath=str(JOB/'renders'/('r'+str(REV)+'-'+tag+'.png'));bpy.ops.render.render(write_still=True)
(JOB/'evidence'/('layout-r'+str(REV)+'.json')).write_text(json.dumps({'revision':REV,'offsets':offsets,'deckDatum':0,'waterDatum':-2.25,'assetStatus':'draft-unverified','sourceMetresPerUnit':1},indent=2))
