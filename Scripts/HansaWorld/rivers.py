"""Offline spline-river migration; preserves the accepted regional source package.

Run after prepare.py. No downloads. Outputs a separately versioned draft.
"""
from pathlib import Path
import json
import sys
import hashlib
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Saved/GenerationJobs/HansaWorld_20260918/python'))
import numpy as np
from scipy.interpolate import PchipInterpolator
from scipy.ndimage import map_coordinates
from scipy.optimize import isotonic_regression
from shapely.geometry import shape, LineString, box
from shapely.ops import transform, unary_union, linemerge
from rasterio.features import rasterize
from rasterio import Affine
from river_geometry import planform, safe_widths

SOURCE = ROOT / 'SourceArt/Terrain/HansaWorld/Prototype_20260918'
OUT = SOURCE / 'SplineRivers_v1'

def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def lines(geom):
    if geom.geom_type == 'LineString':
        yield geom
    elif hasattr(geom, 'geoms'):
        for child in geom.geoms:
            yield from lines(child)

def decreasing(values):
    return isotonic_regression(values, increasing=False).x

def build():
    m = json.loads((SOURCE / 'terrain-manifest.json').read_text(encoding='utf-8'))
    step = m['world_step_m']
    width, height = m['vertices']
    west, south, east, north = m['projected_bounds_m']
    compression = m['compression']
    def warp(e, n, z=None):
        x = (np.asarray(e) - west) / compression
        y = (north - np.asarray(n)) / compression
        for city in m['cities']:
            cx, cy = city['x_cm']/100, city['y_cm']/100
            dx, dy = x-cx, y-cy
            rr = np.hypot(dx, dy)
            radius, core = city['transition_radius_m'], city['core_radius_m']
            grid = np.linspace(0, radius, 4097)
            inverse = PchipInterpolator([0, core, radius], [0, core/compression, radius])
            factor = np.where(rr < radius, np.interp(rr, inverse(grid), grid)/np.maximum(rr, .00001), 1)
            x, y = cx+dx*factor, cy+dy*factor
        return x, y
    def features(name):
        return json.loads((SOURCE / (name+'.geojson')).read_text(encoding='utf-8'))['features']
    land = unary_union([shape(f['geometry']) for f in features('land')])
    lakes = [f for f in features('lakes') if 'Reservoir' not in str(f['properties'].get('featurecla', ''))]
    lake_union = unary_union([shape(f['geometry']) for f in lakes])
    rivers = [f for f in features('rivers') if f['properties'].get('name') and 'canal' not in str(f['properties']).lower()]
    final = np.load(SOURCE / 'terrain-final.npy')
    original_water = np.load(SOURCE / 'water-surface.npy')
    survey = (np.fromfile(SOURCE/'survey-base.r16', dtype='<u2').reshape(height,width).astype(float)-32768)/32
    def sample(grid, xy):
        return map_coordinates(grid, [xy[:,1]/step, xy[:,0]/step], order=1, mode='nearest')
    grouped = {}
    for f in rivers:
        grouped.setdefault(f['properties']['name'], []).append(shape(f['geometry']))
    reaches = []
    for name, geometries in sorted(grouped.items()):
        merged = unary_union(geometries)
        if merged.geom_type == 'MultiLineString':
            merged = linemerge(merged)
        for part in lines(merged.intersection(land).difference(lake_union)):
            geom = transform(warp, part.segmentize(m['projected_step_m']/4)).simplify(1.5)
            if geom.length < 5:
                continue
            # Retain geographical bends; add controls on long straight reaches.
            geom = geom.segmentize(80)
            xy = np.array(geom.coords)[:,:2]
            if len(xy) < 2:
                continue
            z = np.maximum(sample(survey, xy), 0)
            reaches.append({'name': name, 'xy': xy, 'z': z})
    # Shared endpoints have one elevation, independent of source feature order.
    nodes = {}
    for r in reaches:
        for i in (0, -1):
            key = tuple(np.rint(r['xy'][i]*10).astype(int))
            nodes.setdefault(key, []).append((r,i))
    for members in nodes.values():
        xy = members[0][0]['xy'][members[0][1]]
        c,rw = np.rint(xy/step).astype(int)
        patch = original_water[max(0,rw-1):min(height,rw+2),max(0,c-1):min(width,c+2)]
        finite = patch[np.isfinite(patch)]
        z = float(np.median([r['z'][i] for r,i in members]))
        # Preserve sea/lake connection levels when the source endpoint touches them.
        if len(finite) and np.ptp(finite) < .02:
            z = float(np.median(finite))
        for r,i in members:
            r['z'][i] = z
    output = []
    new_terrain = final.copy()
    river_cover = np.zeros(final.shape, dtype=bool)
    for r in reaches:
        xy, z = r['xy'], r['z']
        if z[0] < z[-1]:
            xy, z = xy[::-1].copy(), z[::-1].copy()
        d = np.r_[0, np.cumsum(np.linalg.norm(np.diff(xy,axis=0),axis=1))]
        if np.any(np.diff(d) < .0001):
            continue
        first_z, last_z = z[0], z[-1]
        z = np.clip(decreasing(z), last_z, first_z)
        z[0], z[-1] = first_z, last_z
        # Keep source identity stable when refining the reviewed Trave bends.
        stable = 'Water.River.'+hashlib.sha256((r['name']+repr(np.round(xy,3).tolist())).encode()).hexdigest()[:16]
        refined_tangent = None
        if r['name'] == 'Trave':
            xy, refined_tangent, sample_d, source_d = planform(xy)
            z = PchipInterpolator(source_d,z)(sample_d)
            d = np.r_[0,np.cumsum(np.linalg.norm(np.diff(xy,axis=0),axis=1))]
        # Monotone cubic slopes are explicit: Unreal must not overshoot uphill.
        keys = np.arange(len(d))
        dz = PchipInterpolator(keys,z).derivative()(keys)
        tangent = np.gradient(xy, axis=0)
        tlen = np.linalg.norm(tangent,axis=1)
        local_step = np.gradient(d)
        tangent *= (local_step/np.maximum(tlen,.0001))[:,None]
        if refined_tangent is not None:tangent=refined_tangent
        widths = 36 + 4*(d/d[-1])**2*(3-2*d/d[-1])
        if refined_tangent is not None:widths=safe_widths(xy,tangent,widths)
        points = np.column_stack([xy,z,widths,np.full(len(d),3),tangent,dz])
        # Braided branches can share both endpoints; include the actual path in identity.
        output.append({'id':stable,'name':r['name'],'points':points.round(6).tolist()})
        # Match the cubic surface with a conservative, local bed-only correction.
        dense_d = np.linspace(0,d[-1],max(2,int(d[-1]/4)+1))
        from scipy.interpolate import CubicHermiteSpline
        dense_keys = np.interp(dense_d,d,keys)
        dense_xy = CubicHermiteSpline(keys,xy,tangent)(dense_keys)
        dense_z = PchipInterpolator(keys,z)(dense_keys)
        dense_w = np.interp(dense_d,d,widths)
        for pos, level, full_width in zip(dense_xy,dense_z,dense_w):
            cx,cy = pos/step
            radius = full_width/2+step
            x0,x1 = max(0,int(cx-radius/step)-1),min(width,int(cx+radius/step)+2)
            y0,y1 = max(0,int(cy-radius/step)-1),min(height,int(cy+radius/step)+2)
            yy,xx = np.mgrid[y0:y1,x0:x1]
            distance = np.hypot(xx*step-pos[0],yy*step-pos[1])
            weight = np.clip((radius-distance)/step,0,1)
            # Do not change lake/sea surfaces or dry flattened city cores.
            for city in m['cities']:
                core = np.hypot(xx*step-city['x_cm']/100,yy*step-city['y_cm']/100)<city['core_radius_m']
                weight[core & ~np.isfinite(original_water[y0:y1,x0:x1])] = 0
            target = level-3
            view = new_terrain[y0:y1,x0:x1]
            view[:] = np.minimum(view, view+(target-view)*weight)
            river_cover[y0:y1,x0:x1] |= distance < radius
    # Reuse only lake/sea cells from the old inland mesh; never leave river tiles under the splines.
    lake_world = [(transform(warp,shape(f['geometry']).segmentize(m['projected_step_m']/4)),1) for f in lakes]
    lake_mask = rasterize(lake_world,out_shape=final.shape,transform=Affine(step,0,-step/2,0,step,-step/2),dtype='uint8')!=0
    cells = np.fromfile(SOURCE/'water-cells.f32',dtype='<f4').reshape(-1,3)
    c, rr = cells[:,0].astype(int),cells[:,1].astype(int)
    keep = lake_mask[rr,c] | lake_mask[rr+1,c] | lake_mask[rr,c+1] | lake_mask[rr+1,c+1]
    OUT.mkdir(parents=True,exist_ok=True)
    cells[keep].tofile(OUT/'lake-cells.f32')
    delta = np.rint((new_terrain-final)*32+32768)
    if delta.min()<0 or delta.max()>65535:
        raise ValueError('Riverbed correction exceeds Landscape encoding')
    delta.astype('<u2').tofile(OUT/'riverbed-delta.r16')
    np.rint(new_terrain*32+32768).astype('<u2').tofile(OUT/'terrain-final.r16')
    manifest = {'schema_version':1,'map':m['map'],'point_columns':['x_m','y_m','z_m','full_width_m','depth_m','tangent_x_m','tangent_y_m','tangent_z_m'],
        'source_manifest_sha256':sha(SOURCE/'terrain-manifest.json'),'rivers':output,
        'assumptions':['Modern Natural Earth and OSM paths; not verified medieval hydrography.',
            'Direction inferred from endpoint elevation; tidal/flat reaches remain uncertain.',
            'Nominal widths 36 to 40 game metres; reviewed Trave bends use curvature-limited widths and bounded 12m planform smoothing. Depth 3m is inferred gameplay, not surveyed bathymetry.',
            'Automatic Water brush disabled; deterministic bed correction is isolated in SplineRiver_Hydrology.'],
        'source_hashes':{n:sha(SOURCE/n) for n in ['rivers.geojson','lakes.geojson','land.geojson','terrain-final.r16']},
        'files':{n:sha(OUT/n) for n in ['lake-cells.f32','riverbed-delta.r16','terrain-final.r16']},
        'lake_cells':int(keep.sum()),'removed_river_cells':int((~keep).sum()),
        'maximum_bed_lowering_m':float((final-new_terrain).max())}
    (OUT/'rivers.json').write_text(json.dumps(manifest,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
    print(json.dumps({k:v for k,v in manifest.items() if k not in ('rivers','source_hashes','files')},indent=2))
    print('Rivers:',len(output),'controls:',sum(len(r['points']) for r in output))

if __name__=='__main__':
    build()
