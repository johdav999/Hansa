"""Reproducible offline Hansa campaign geography. Source downloads are opt-in.

Run: python Scripts/HansaWorld/prepare.py --download
Dependencies live in the job-local python directory, never Unreal's interpreter.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import sys
import urllib.request
import zipfile
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[2]
JOB = ROOT / 'Saved/GenerationJobs/HansaWorld_20260918'
sys.path.insert(0, str(JOB / 'python'))
import numpy as np
import rasterio
from rasterio.features import rasterize
from rasterio.transform import from_origin
from rasterio.warp import reproject, Resampling
from scipy.ndimage import map_coordinates, distance_transform_edt
from scipy.interpolate import PchipInterpolator
from pyproj import Transformer
import shapefile
from shapely.geometry import shape, mapping, box, Point
from shapely.ops import transform as transform_shape

OUT = ROOT / 'SourceArt/Terrain/HansaWorld/Prototype_20260918'
SOURCE = OUT / 'sources'
CONFIG = json.loads(Path(__file__).with_name('world.json').read_text(encoding='utf-8'))
ETOPO = 'https://www.ngdc.noaa.gov/mgg/global/relief/ETOPO2022/data/60s/60s_bed_elev_gtif/ETOPO_2022_v1_60s_N90W180_bed.tif'
DATA = {
    'etopo.tif': ETOPO,
    'land.zip': 'https://naturalearth.s3.amazonaws.com/10m_physical/ne_10m_land.zip',
    'lakes.zip': 'https://naturalearth.s3.amazonaws.com/10m_physical/ne_10m_lakes.zip',
    'rivers.zip': 'https://naturalearth.s3.amazonaws.com/10m_physical/ne_10m_rivers_lake_centerlines.zip',
}

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def save_json(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False), encoding='utf-8')

def download():
    SOURCE.mkdir(parents=True, exist_ok=True)
    records = []
    for name, url in DATA.items():
        destination = SOURCE / name
        if not destination.exists():
            print('Downloading', name, flush=True)
            request = urllib.request.Request(url, headers={'User-Agent': 'HansaTerrainPrototype/1.0'})
            with urllib.request.urlopen(request, timeout=90) as response, destination.with_suffix('.part').open('wb') as output:
                while chunk := response.read(4 * 1024 * 1024):
                    output.write(chunk)
            destination.with_suffix('.part').rename(destination)
        records.append({'file': name, 'url': url, 'sha256': digest(destination), 'bytes': destination.stat().st_size,
                        'accessed_utc': datetime.now(timezone.utc).isoformat(),
                        'license': 'NOAA ETOPO: free private, academic and commercial use' if name == 'etopo.tif' else 'Natural Earth: public domain'})
    save_json(SOURCE / 'sources.json', records)

def features(name, project, bounds):
    with zipfile.ZipFile(SOURCE / (name + '.zip')) as archive:
        files = {Path(n).suffix: n for n in archive.namelist()}
        with archive.open(files['.shp']) as shp, archive.open(files['.shx']) as shx, archive.open(files['.dbf']) as dbf:
            reader = shapefile.Reader(shp=shp, shx=shx, dbf=dbf, encoding='utf-8')
            result = []
            for record in reader.iterShapeRecords():
                geom = shape(record.shape.__geo_interface__)
                if not geom.intersects(box(-15, 45, 45, 72)):
                    continue
                geom = transform_shape(project.transform, geom).intersection(bounds)
                if not geom.is_empty:
                    result.append((geom, record.record.as_dict()))
            return result

def encode(array, path):
    # Exact native Unreal equation: (code - 32768) * ZScale / 128.
    code = np.rint(array * 100 * 128 / 400 + 32768)
    if code.min() < 0 or code.max() > 65535:
        raise ValueError('Elevation outside declared encoding range')
    code.astype('<u2').tofile(path)

def build():
    for name in DATA:
        if not (SOURCE / name).exists():
            raise RuntimeError('Missing source; run --download first: ' + name)
    OUT.mkdir(parents=True, exist_ok=True)
    width, height = CONFIG['vertices']
    west, south, east, north = CONFIG['projected_bounds_m']
    # Match rectangular Landscape topology with isotropic spacing.
    step = (east - west) / (width - 1)
    south = north - (height - 1) * step
    compression = CONFIG['compression']
    world_step = step / compression
    projected_transform = from_origin(west - step / 2, north + step / 2, step, step)
    x = np.arange(width, dtype='float32') * step + west
    y = north - np.arange(height, dtype='float32') * step
    project = Transformer.from_crs('EPSG:4326', CONFIG['crs'], always_xy=True)
    cities = []
    for stable, label, lon, lat, kind in CONFIG['cities']:
        e, n = project.transform(lon, lat)
        assert west < e < east and south < n < north, label
        cities.append({'id': 'City.' + stable, 'name': label, 'longitude': lon, 'latitude': lat,
                       'easting': e, 'northing': n, 'kind': kind,
                       'coordinate_confidence': 'approximate historical centre; editorial seed, requires gazetteer/archaeological review'})
    # Disjoint radial warps: preserve every city coordinate while expanding its local geometry.
    rows, cols = np.indices((height, width), dtype='float32')
    for city in cities:
        nearest = min(np.hypot(city['easting'] - c['easting'], city['northing'] - c['northing']) for c in cities if c is not city)
        radius = min(1400.0, nearest / compression * .44)
        core = min(220.0, radius * .22)
        cx, cy = (city['easting'] - west) / step, (north - city['northing']) / step
        dx, dy = (cols - cx) * world_step, (rows - cy) * world_step
        distance = np.hypot(dx, dy)
        inside = distance < radius
        inverse = PchipInterpolator([0, core, radius], [0, core / compression, radius])
        ratio = inverse(distance[inside]) / np.maximum(distance[inside], .0001)
        rows[inside] = cy + (rows[inside] - cy) * ratio
        cols[inside] = cx + (cols[inside] - cx) * ratio
        city.update(core_radius_m=core, transition_radius_m=radius, x_cm=cx * world_step * 100, y_cm=cy * world_step * 100)
    coordinates = np.array([rows, cols])
    del rows, cols
    base = np.full((height, width), -99999, dtype='float32')
    with rasterio.open(SOURCE / 'etopo.tif') as source:
        reproject(rasterio.band(source, 1), base, src_transform=source.transform, src_crs=source.crs,
                  dst_transform=projected_transform, dst_crs=CONFIG['crs'], resampling=Resampling.bilinear,
                  dst_nodata=-99999, num_threads=4)
    assert np.isfinite(base).all() and base.min() > -99999
    with rasterio.open(OUT / 'survey-projected.tif', 'w', driver='GTiff', width=width, height=height, count=1,
                       dtype='float32', crs=CONFIG['crs'], transform=projected_transform, compress='deflate') as output:
        output.write(base, 1)
    # Vertical compression is separately declared; no measured-data file is altered.
    survey = map_coordinates(base, coordinates, order=1, mode='nearest') / 8
    bounds = box(west, south, east, north)
    all_features = {n: features(n, project, bounds) for n in ['land', 'lakes', 'rivers']}
    supplemental=SOURCE/'supplemental-rivers.geojson'
    if supplemental.exists():
        for feature in json.loads(supplemental.read_text(encoding='utf-8'))['features']:
            geom=transform_shape(project.transform,shape(feature['geometry'])).intersection(bounds)
            if not geom.is_empty: all_features['rivers'].append((geom,feature['properties']))
    land = rasterize([(g, 1) for g, _ in all_features['land']], out_shape=base.shape, transform=projected_transform, dtype='uint8')
    land = map_coordinates(land, coordinates, order=0, mode='nearest') != 0
    lake_geoms = [(g, p) for g, p in all_features['lakes'] if 'Reservoir' not in str(p.get('featurecla', ''))]
    lake_ids = rasterize([(g, i + 1) for i, (g, _) in enumerate(lake_geoms)], out_shape=base.shape,
                         transform=projected_transform, dtype='uint16')
    lake_ids = map_coordinates(lake_ids, coordinates, order=0, mode='nearest')
    river_geoms = [(g, p) for g, p in all_features['rivers'] if p.get('name') and 'canal' not in str(p).lower()]
    def world_coordinates(e, n, z=None):
        wx=(np.asarray(e)-west)/compression
        wy=(north-np.asarray(n))/compression
        for city in cities:
            dx=wx-city['x_cm']/100; dy=wy-city['y_cm']/100
            rr=np.hypot(dx,dy); radius=city['transition_radius_m']; core=city['core_radius_m']
            grid=np.linspace(0,radius,4097)
            inverse=PchipInterpolator([0,core,radius],[0,core/compression,radius])
            warped=np.interp(rr,inverse(grid),grid)
            factor=np.where(rr<radius,warped/np.maximum(rr,.00001),1)
            wx=city['x_cm']/100+dx*factor; wy=city['y_cm']/100+dy*factor
        return wx,wy
    # Buffer after the city warp so harbour channels do not expand across whole cities.
    world_transform=rasterio.Affine(world_step,0,-world_step/2,0,world_step,-world_step/2)
    world_rivers=[transform_shape(world_coordinates,g.segmentize(step/4)) for g,p in river_geoms]
    river_mask = rasterize([(g.buffer(20),1) for g in world_rivers],out_shape=base.shape,
                          transform=world_transform,dtype='uint8') != 0
    del coordinates
    final = survey.copy()
    # Evidence coastline masks override coarse DEM shoreline ambiguity in a separate delta.
    final[~land] = np.minimum(final[~land], -3)
    final[land] = np.maximum(final[land], 1.5)
    water = np.full(base.shape, np.nan, dtype='float32')
    water[~land] = 0
    water_inventory = []
    for i, (geom, props) in enumerate(lake_geoms, 1):
        mask = lake_ids == i
        if not mask.any():
            continue
        elevation = max(0, float(np.median(survey[mask])))
        water[mask] = elevation
        final[mask] = elevation - 3
        water_inventory.append({'id': 'Water.Lake.' + str(i), 'name': props.get('name') or 'Unnamed lake',
                                'type': 'lake', 'surface_m': elevation, 'surface_source': 'inferred from ETOPO median',
                                'depth_m': 3, 'depth_source': 'gameplay', 'source': 'Natural Earth 10m lakes'})
    river_only = river_mask & land & (lake_ids == 0)
    water[river_only] = np.maximum(survey[river_only], 1)
    final[river_only] = water[river_only] - 3
    hydrology = final.copy()
    # Broad, smooth grading is dry-land-only; waterways are protected.
    gx, gy = np.meshgrid(np.arange(width) * world_step, np.arange(height) * world_step)
    for city in cities:
        distance = np.hypot(gx - city['x_cm']/100, gy - city['y_cm']/100)
        radius = city['core_radius_m']
        core_mask = (distance < radius) & np.isnan(water)
        assert core_mask.any(), 'No buildable land at ' + city['name']
        target = max(3.0, float(np.median(final[core_mask])))
        t = np.clip((distance - radius) / max(radius * 2, world_step), 0, 1)
        weight = 1 - t*t*t*(t*(t*6 - 15) + 10)
        weight[~np.isnan(water)] = 0
        before = final.copy()
        final += weight * (target - final)
        city.update(target_elevation_m=target, maximum_grading_delta_m=float(np.abs(final-before).max()))
        r = int(round(city['y_cm']/100/world_step)); c = int(round(city['x_cm']/100/world_step))
        city['z_cm'] = float(final[r,c] * 100)
        city['centre_is_dry'] = bool(np.isnan(water[r,c]))
        # Do not move a marker or forge land when a coarse shoreline disagrees with the historical location.
    del gx, gy
    encode(survey, OUT / 'survey-base.r16')
    encode(final, OUT / 'terrain-final.r16')
    encode(hydrology - survey, OUT / 'hydrology-delta.r16')
    encode(final - hydrology, OUT / 'gameplay-delta.r16')
    # Shore mask and foliage occupancy are spatial data, not newly generated raster artwork.
    dry = np.isnan(water)
    shore_distance = distance_transform_edt(dry) * world_step
    shore = np.clip(255 - shore_distance/80 * 255, 0, 255).astype('uint8')
    shore.tofile(OUT / 'shore.u8')
    np.save(OUT / 'water-surface.npy', water)
    np.save(OUT / 'terrain-final.npy', final)
    # Structured cell surfaces for native custom Water bodies (preserve islands/holes).
    water_cells = []
    for r in range(height-1):
        for c in range(width-1):
            block = water[r:r+2, c:c+2]
            if land[r,c] and np.isfinite(block).all():
                water_cells.append([c, r, float(np.mean(block))])
    np.asarray(water_cells, dtype='<f4').tofile(OUT / 'water-cells.f32')
    rng = np.random.default_rng(CONFIG['seed'])
    trees = []
    # Deterministic artistic forest patches; this draft does not claim measured medieval forest cover.
    for _ in range(180000):
        c, r = int(rng.integers(2, width-2)), int(rng.integers(2, height-2))
        xw, yw = c*world_step, r*world_step
        if not dry[r,c] or shore_distance[r,c] < 35 or final[r,c] > 180:
            continue
        if any(np.hypot(xw-city['x_cm']/100, yw-city['y_cm']/100) < city['core_radius_m']*1.8 for city in cities):
            continue
        patch = np.sin(xw/470)*np.cos(yw/530) + .5*np.sin((xw+yw)/170)
        if patch < .4:
            continue
        slope = max(abs(final[r+1,c]-final[r-1,c]), abs(final[r,c+1]-final[r,c-1])) / (world_step*2)
        if slope > .27:
            continue
        species = int(rng.choice([0,1,2,3,4], p=[.08,.05,.27,.35,.25]))
        trees.append([xw*100,yw*100,float(final[r,c]*100),float(rng.uniform(0,360)),float(rng.uniform(.9,1.1)),species,int(rng.random()<.31)])
    np.asarray(trees,dtype='<f4').tofile(OUT/'trees.f32')
    for name, feats in all_features.items():
        save_json(OUT / (name + '.geojson'), {'type':'FeatureCollection','crs':{'type':'name','properties':{'name':CONFIG['crs']}},
                  'features':[{'type':'Feature','geometry':mapping(g),'properties':p} for g,p in feats]})
    manifest = {**CONFIG, 'projected_bounds_m':[west,south,east,north], 'source_vertical_datum':'EGM2008 (EPSG:3855)',
                'source_resolution_arcseconds':60, 'projected_step_m':step, 'world_step_m':world_step,
                'vertical_compression':8, 'world_size_m':[(width-1)*world_step,(height-1)*world_step],
                'unreal_axes':'X east; Y south; Z up', 'encoding':{'z_scale':400,'actor_z_cm':0,'native_decode':'(uint16-32768)/128*400 cm'},
                'cities':cities,'tree_count':len(trees),'water_cell_count':len(water_cells), 'water_cell_stride':1,
                'water_inventory':water_inventory,'river_names':sorted(set(p['name'] for g,p in river_geoms)),
                'sources':json.loads((SOURCE/'sources.json').read_text()),
                'limitations':['Modern regional data; historical corrections await evidence.',
                               'Forest patches are an artistic draft, not measured land cover.',
                               'River widths and depths are exaggerated for gameplay; flow and shipping connectivity require validation.',
                               'Natural Earth 1:10 million omits small rivers/islands; city hydrography requires refinement.',
                               'City coordinates are approximate editorial seeds.',
                               'City radial warp is a gameplay transform; GeoReferencing alone cannot invert it.'],
                'files':{p.name:digest(p) for p in OUT.iterdir() if p.suffix in ('.r16','.u8','.f32','.geojson','.tif')}}
    save_json(OUT/'terrain-manifest.json',manifest)
    with (OUT/'city-centres.csv').open('w',encoding='utf-8-sig',newline='') as stream:
        writer=csv.DictWriter(stream,fieldnames=list(cities[0]));writer.writeheader();writer.writerows(cities)
    print(json.dumps({'world_size_m':manifest['world_size_m'],'cities':len(cities),'trees':len(trees),
                      'water_cells':len(water_cells),'wet_city_centres':[c['name'] for c in cities if not c['centre_is_dry']],
                      'height_range_m':[float(final.min()),float(final.max())]},indent=2),flush=True)

if __name__ == '__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--download',action='store_true');parser.add_argument('--download-only',action='store_true')
    args=parser.parse_args()
    if args.download or args.download_only: download()
    if not args.download_only: build()
