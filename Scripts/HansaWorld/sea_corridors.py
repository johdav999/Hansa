"""Offline, reversible sea-plane cutouts limited to the approved inland rivers."""
import json
import rivers
import numpy as np
from scipy.interpolate import CubicHermiteSpline, PchipInterpolator
from shapely import constrained_delaunay_triangles
from shapely.geometry import shape, LineString, box
from shapely.ops import transform, unary_union

OUT = rivers.OUT / 'SeaCorridors_v1'

def build():
    m = json.loads((rivers.SOURCE/'terrain-manifest.json').read_text(encoding='utf-8'))
    r = json.loads((rivers.OUT/'rivers.json').read_text(encoding='utf-8'))
    west, south, east, north = m['projected_bounds_m']
    compression = m['compression']
    step = m['world_step_m']
    def warp(e, n, z=None):
        x, y = (np.asarray(e)-west)/compression, (north-np.asarray(n))/compression
        for city in m['cities']:
            cx, cy = city['x_cm']/100, city['y_cm']/100
            dx, dy = x-cx, y-cy
            rr = np.hypot(dx,dy)
            radius, core = city['transition_radius_m'], city['core_radius_m']
            grid = np.linspace(0,radius,4097)
            inverse = PchipInterpolator([0,core,radius],[0,core/compression,radius])
            factor = np.where(rr<radius,np.interp(rr,inverse(grid),grid)/np.maximum(rr,.00001),1)
            x,y = cx+dx*factor,cy+dy*factor
        return x,y
    def geo(name):
        features=json.loads((rivers.SOURCE/(name+'.geojson')).read_text(encoding='utf-8'))['features']
        return unary_union([transform(warp,shape(f['geometry']).segmentize(m['projected_step_m']/4))
            for f in features if name!='lakes' or 'Reservoir' not in str(f['properties'].get('featurecla',''))])
    land, lakes = geo('land'), geo('lakes')
    corridors=[]
    for reach in r['rivers']:
        p=np.array(reach['points'])
        keys=np.arange(len(p))
        length=np.linalg.norm(np.diff(p[:,:2],axis=0),axis=1).sum()
        t=np.linspace(0,len(p)-1,max(len(p)*8,int(length/4)+1))
        curve=CubicHermiteSpline(keys,p[:,:2],p[:,5:7])(t)
        # Includes the coarse former channel shoulders, not just the new ribbon.
        corridors.append(LineString(curve).buffer(float(p[:,3].max()/2+3*step),quad_segs=4))
    corridor=unary_union(corridors)
    allowed=land.difference(lakes)
    cut= corridor.intersection(allowed)
    assert cut.difference(allowed).area<.001
    assert cut.intersection(lakes).area<.001
    OUT.mkdir(parents=True,exist_ok=True)
    wx,wy=(m['vertices'][0]-1)*step,(m['vertices'][1]-1)*step
    entries=[]
    for x in range(8):
        for y in range(6):
            tile=box(x*wx/8,y*wy/6,(x+1)*wx/8,(y+1)*wy/6)
            removed=tile.intersection(cut)
            if removed.is_empty:continue
            kept=tile.difference(cut)
            tris=list(constrained_delaunay_triangles(kept).geoms)
            assert abs(sum(t.area for t in tris)-kept.area)<max(.01,kept.area*1e-9)
            assert abs(kept.area+removed.area-tile.area)<.01
            vertices=[]
            for tri in tris:
                assert tri.difference(kept).area<.0001
                xy=np.array(tri.exterior.coords)[:3,:2]
                a,b=xy[1]-xy[0],xy[2]-xy[0]
                if a[0]*b[1]-a[1]*b[0]>0:xy=xy[::-1]
                vertices.extend(np.column_stack([xy,np.zeros(3)]).tolist())
            name=f'SM_Sea_RiverCut_v1_{x:02}_{y:02}'
            file=name+'.f32'
            np.asarray(vertices,dtype='<f4').tofile(OUT/file)
            entries.append({'actor':f'SM_Sea_{x:02}_{y:02}','mesh':name,'file':file,
                'vertices':len(vertices),'removed_area_m2':removed.area,'sha256':rivers.sha(OUT/file)})
    manifest={'schema_version':1,'map':m['map'],'river_sha256':rivers.sha(rivers.OUT/'rivers.json'),
        'source_hashes':{n:rivers.sha(rivers.SOURCE/n) for n in ['land.geojson','lakes.geojson','terrain-manifest.json']},
        'policy':'Subtract only buffered spline corridors intersected with geographical land excluding lakes. Original assets retained.',
        'removed_area_m2':cut.intersection(box(0,0,wx,wy)).area,'tiles':entries}
    (OUT/'sea-corridors.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print('Validated sea cutouts:',len(entries),'tiles;',sum(t['vertices']//3 for t in entries),'triangles')

if __name__=='__main__':build()
