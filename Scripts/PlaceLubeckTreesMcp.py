"""Deterministic, resumable placement of approved tree assets through Unreal MCP.

Run from the repository root: python Scripts/PlaceLubeckTreesMcp.py --plan
Then --limit 20 for an initial inspection, or --limit 320 to finish this manifest.
This authors map actors only; it does not create simulation resources.
"""
from pathlib import Path
import sys,json,random,math,argparse,collections
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'SourceArt/Generated/Trees/LubeckSummer/v1/scripts'))
JOB=ROOT/'Saved/GenerationJobs/LubeckTreePlacement_20260916';JOB.mkdir(exist_ok=True)
LEVEL='/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/L_Lubeck_Terrain_Preview_WP'
TAG='HansaTree.LubeckPlacement.20260916'
SURVEY=ROOT/'SourceArt/Terrain/Lubeck/Survey_20260907'
water=json.loads((SURVEY/'hydrology/water-build.json').read_text())['bodies']
segments=[];lakes=[]
for b in water:
    p=np.asarray(b['points'],float)
    if b['type']=='lake':lakes.append(p[:,:2])
    else:
        for a,z in zip(p[:-1],p[1:]):segments.append((a[0],a[1],z[0],z[1],max(a[3],z[3])/2))
segs=np.asarray(segments)
def water_distance(x,y):
    dx=segs[:,2]-segs[:,0];dy=segs[:,3]-segs[:,1]
    t=np.clip(((x-segs[:,0])*dx+(y-segs[:,1])*dy)/np.maximum(dx*dx+dy*dy,1),0,1)
    result=float(np.min(np.hypot(x-segs[:,0]-t*dx,y-segs[:,1]-t*dy)-segs[:,4]))
    for p in lakes:
        a=p;b=np.roll(p,-1,axis=0);dx=b[:,0]-a[:,0];dy=b[:,1]-a[:,1]
        t=np.clip(((x-a[:,0])*dx+(y-a[:,1])*dy)/np.maximum(dx*dx+dy*dy,1),0,1)
        distance=float(np.min(np.hypot(x-a[:,0]-t*dx,y-a[:,1]-t*dy)))
        inside=bool(np.count_nonzero(((a[:,1]>y)!=(b[:,1]>y)) & (x<(b[:,0]-a[:,0])*(y-a[:,1])/np.where(abs(dy)<.001,.001,dy)+a[:,0]))%2)
        result=min(result,-distance if inside else distance)
    return result
meta=json.loads((SURVEY/'terrain-manifest.json').read_text())
height=np.fromfile(SURVEY/meta['heightmap_file'],dtype='<u2').reshape(2017,2017).astype(float)/128*25-3200
def sample(x,y):
    col=int(round((x+201550)/200));row=int(round((y+201650)/200))
    return height[row,col]
def plan():
    rng=random.Random(20260916);trees=[]
    # Founding-bank woods plus distant groves; retain a 60m construction clearing.
    patches=[(-48500,15500,6500,38),(-52000,-8500,6500,38),(-60000,4000,7000,34),(-45500,-19000,6000,32),(-25500,26000,7000,30),(-67000,24500,7000,28),(-125000,-75000,11000,20),(140000,40000,10000,20),(-110000,100000,12000,20),(110000,110000,10000,20),(75000,-140000,10000,20),(-65000,-125000,10000,20)]
    for pi,(cx,cy,radius,count) in enumerate(patches):
        made=0
        for attempt in range(8000):
            a=rng.random()*math.tau;r=radius*math.sqrt(rng.random());x=cx+math.cos(a)*r;y=cy+math.sin(a)*r
            if math.hypot(x+38600,y-4600)<6000:continue
            shore=water_distance(x,y)
            if shore<1300:continue
            h=sample(x,y);slope=max(abs(sample(x+400,y)-sample(x-400,y)),abs(sample(x,y+400)-sample(x,y-400)))/800
            if h<130 or slope>math.tan(math.radians(15)):continue
            if any((x-t['x'])**2+(y-t['y'])**2<850**2 for t in trees):continue
            species=rng.choices(['Alder','Willow','Birch','Oak','Beech'],weights=[40,35,15,6,4] if shore<7000 else [0,0,24,42,34])[0]
            age='Young' if rng.random()<.31 else 'Mature';idx=len(trees)+1
            trees.append(dict(id=f'LubeckTree_{idx:04d}',species=species,age=age,patch=pi+1,x=round(x,2),y=round(y,2),source_z=round(h,2),water_clearance_cm=round(shore,2),source_slope_degrees=round(math.degrees(math.atan(slope)),2),yaw=round(rng.random()*360,2),scale=round(rng.uniform(.90,1.10),3),mesh=f'/Game/Hansa/Generated/Staging/LubeckTrees_20260916/Meshes/SM_{species}_{age}'))
            made+=1
            if made==count:break
        assert made==count,(pi,made,count)
    (JOB/'placement_plan.json').write_text(json.dumps({'level':LEVEL,'tag':TAG,'seed':20260916,'trees':trees},indent=2))
    print('PLAN',len(trees),dict(collections.Counter(t['species']+'_'+t['age'] for t in trees)),flush=True)
def place(limit):
    from ue_batch import call
    S='editor_toolset.toolsets.scene.SceneTools';A='editor_toolset.toolsets.actor.ActorTools';O='editor_toolset.toolsets.object.ObjectTools'
    assert call(S,'get_current_level')==LEVEL
    assert not call('EditorToolset.EditorAppToolset','IsPIERunning')
    trees=json.loads((JOB/'placement_plan.json').read_text())['trees']
    statefile=JOB/'placed_trees.json';done=json.loads(statefile.read_text()) if statefile.exists() else []
    existing=call(S,'find_actors',name='',tag=TAG,collision_channels=[])
    labels={call(A,'get_label',actor=a):a for a in existing}
    for t in trees[:limit]:
        label=t['id']+'_'+t['species']+'_'+t['age']
        if label in labels:
            if not any(d['id']==t['id'] for d in done):
                tr=call(A,'get_actor_transform',actor=labels[label]);done.append(dict(**t,label=label,z=tr['location']['z'],actor=labels[label]));statefile.write_text(json.dumps(done,indent=2))
            continue
        # Real collision trace, after source water/slope/spacing rejection.
        distance=call(S,'trace_world',start={'x':t['x'],'y':t['y'],'z':t['source_z']+100},end={'x':t['x'],'y':t['y'],'z':t['source_z']-200})
        assert distance is not None and 0<=distance<=300,(label,distance)
        z=t['source_z']+100-distance
        assert abs(z-t['source_z'])<150,(label,'surface differs from survey',z,t['source_z'])
        actor=call(S,'add_to_scene_from_asset',asset_path=t['mesh'],name=label,xform={'location':{'x':t['x'],'y':t['y'],'z':z},'rotation':{'pitch':0,'yaw':t['yaw'],'roll':0},'scale':{'x':t['scale'],'y':t['scale'],'z':t['scale']}})
        assert actor and actor['refPath'].startswith(LEVEL+'.')
        call(O,'set_properties',instance=actor,values=json.dumps({'tags':[TAG,t['id'],'Tree.Species.'+t['species'],'Tree.Age.'+t['age']]}))
        call(S,'set_actor_folder',actor=actor,folder_path='Environment/Trees/LubeckSummer/'+t['species'])
        done.append(dict(**t,label=label,z=z,actor=actor));statefile.write_text(json.dumps(done,indent=2))
        if len(done)%20==0:print('PLACED_SAVED',len(done),flush=True)
    context=json.loads(call('HansaEditor.HansaCityTerrainToolset','InspectAuthoringContext'))
    assert all('LubeckTerrain_20260907/L_Lubeck_Terrain_Preview_WP' in p for p in context['dirtyPackages']),context['dirtyPackages']
    call('editor_toolset.toolsets.asset.AssetTools','save_assets',asset_paths=[]);print('MAP_SAVED',len(done),flush=True)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--plan',action='store_true');p.add_argument('--limit',type=int,default=320);a=p.parse_args()
    if a.plan:plan()
    else:place(a.limit)
