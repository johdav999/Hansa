"""Acquire official bare-earth DGM1; encode a reversible modern survey, not medieval terrain."""
import hashlib, json, sys, urllib.parse, urllib.request
from pathlib import Path
REPO=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(REPO/'Saved/GenerationJobs/LubeckTerrain_20260907/python-deps'))
import numpy as np
import rasterio
from rasterio.transform import from_origin
from pyproj import Transformer

OUT=REPO/'SourceArt/Terrain/Rostock/Survey_20260908'
OUT.mkdir(parents=True,exist_ok=True)
# Includes the old town and Unterwarnow waterfront, not the remote Baltic coast.
cx,cy=[round(v/2)*2 for v in Transformer.from_crs(4326,25833,always_xy=True).transform(12.14,54.09)]
x0,y0,x1,y1=cx-2016,cy-2016,cx+2017,cy+2017
params=[('SERVICE','WCS'),('REQUEST','GetCoverage'),('VERSION','2.0.1'),('COVERAGEID','mv_dgm'),
        ('FORMAT','image/tiff'),('SUBSET',f'x({x0},{x1})'),('SUBSET',f'y({y0},{y1})')]
url='https://www.geodaten-mv.de/dienste/dgm_wcs?'+urllib.parse.urlencode(params)
source=OUT/'rostock-dgm1-original.tif'
if not source.exists():
    with urllib.request.urlopen(url,timeout=60) as response:payload=response.read(100_000_001)
    if len(payload)>100_000_000:raise RuntimeError('Source exceeds bounded 100MB acquisition')
    # Preserve the downloaded original. Do not execute any supplier code.
    source.write_bytes(payload)
with rasterio.open(source) as ds:
    assert ds.crs.to_epsg()==25833 and ds.count==1 and ds.res==(1,1)
    xs=x0+.5+np.arange(2017)*2;ys=y1-.5-np.arange(2017)*2
    assert xs[-1]<ds.bounds.right and ys[-1]>=ds.bounds.bottom
    points=[(x,y) for y in ys for x in xs]
    grid=np.fromiter((v[0] for v in ds.sample(points)),dtype=np.float32,count=len(points)).reshape(2017,2017)
    assert np.isfinite(grid).all()
    if ds.nodata is not None:assert not np.any(grid==ds.nodata)
    original={'width':ds.width,'height':ds.height,'nodata':ds.nodata,'transform':list(ds.transform),'crs':str(ds.crs)}
assert grid.min()>-32 and grid.max()<96
codes=np.rint((grid.astype(np.float64)+32)/128*65536).astype('<u2')
encoded=OUT/'rostock-survey--2017x2017--2m.r16';codes.tofile(encoded)
with rasterio.open(OUT/'rostock-survey.tif','w',driver='GTiff',width=2017,height=2017,count=1,dtype='float32',crs='EPSG:25833',transform=from_origin(xs[0]-1,ys[0]+1,2,2),compress='deflate') as target:target.write(grid,1)
manifest={'schema_version':1,'city_id':'City.Rostock','status':'modern-survey-source-only',
 'historical_target':'Late medieval city; no historical or gameplay corrections applied',
 'source_url':url,'publisher':'LAiV Mecklenburg-Vorpommern','accessed':'2026-09-08',
 'license':'CC BY 4.0','attribution':'GeoBasis-DE/M-V',
 'license_url':'https://www.laiv-mv.de/Geoinformation/Open_Data_Angebot/',
 'metadata_url':'https://www.laiv-mv.de/Geoinformation/Geobasisdaten/Gelaendemodelle/',
 'source_crs':'EPSG:25833','source_vertical_datum':'DE_DHHN2016_NH EPSG:7837',
 'acquisition_date':'Not exposed by WCS coverage; tile-level acquisition lookup remains open',
 'source':original,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
 'origin_projected_m':[cx,cy,0],'bounds_projected_m':[float(xs[0]),float(ys[-1]),float(xs[-1]),float(ys[0])],
 'width_vertices':2017,'height_vertices':2017,'metres_per_vertex':2,'unreal_axes':'X east; Y south; Z up',
 'resampling':'Exact aligned DGM1 cell-center decimation every second source sample; no interpolation',
 'encoded_min_elevation_m':-32,'encoded_max_elevation_m':96,'landscape_xy_scale_cm':200,
 'landscape_z_scale':25,'landscape_actor_z_cm':3200,'component_quads':126,'components_x':16,'components_y':16,
 'heightmap_file':encoded.name,'heightmap_sha256':hashlib.sha256(encoded.read_bytes()).hexdigest(),
 'minimum_m':float(grid.min()),'maximum_m':float(grid.max()),'nodata_count':0,
 'encoding':'UE LandscapeDataAccess little-endian uint16; 65536-step nominal interval',
 'maximum_encoding_error_m':float(np.max(np.abs(codes.astype(float)/65536*128-32-grid))),
 'control_points':[{'row':r,'column':c,'easting':float(xs[c]),'northing':float(ys[r]),'height_m':float(grid[r,c])} for r,c in ((0,0),(0,2016),(1008,1008),(2016,0),(2016,2016))],
 'remaining':['Tile acquisition date','Medieval shoreline and modern reclamation review','Hydrology and bathymetry','Unreal import and control points','PBR and layers','Visual QA, streaming and Shipping']}
(OUT/'terrain-manifest.json').write_text(json.dumps(manifest,indent=2))
print(json.dumps(manifest,indent=2))
